/* Разместите здесь код класса Vector*/
#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T> class RawMemory {
public:
  RawMemory() = default;

  explicit RawMemory(size_t capacity)
      : buffer_(Allocate(capacity)), capacity_(capacity) {}

  ~RawMemory() { Deallocate(buffer_); }

  T *operator+(size_t offset) noexcept {
    // Разрешается получать адрес ячейки памяти, следующей за последним
    // элементом массива
    assert(offset <= capacity_);
    return buffer_ + offset;
  }

  const T *operator+(size_t offset) const noexcept {
    return const_cast<RawMemory &>(*this) + offset;
  }

  const T &operator[](size_t index) const noexcept {
    return const_cast<RawMemory &>(*this)[index];
  }

  T &operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
  }

  void Swap(RawMemory &other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
  }

  const T *GetAddress() const noexcept { return buffer_; }

  T *GetAddress() noexcept { return buffer_; }

  size_t Capacity() const { return capacity_; }

private:
  // Выделяет сырую память под n элементов и возвращает указатель на неё
  static T *Allocate(size_t n) {
    return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
  }

  // Освобождает сырую память, выделенную ранее по адресу buf при помощи
  // Allocate
  static void Deallocate(T *buf) noexcept { operator delete(buf); }

  T *buffer_ = nullptr;
  size_t capacity_ = 0;
};

template <typename T> class Vector {
public:
  using iterator = T *;
  using const_iterator = const T *;

  iterator begin() noexcept { return data_.GetAddress(); }
  iterator end() noexcept { return data_.GetAddress() + size_; }
  const_iterator begin() const noexcept { return data_.GetAddress(); }
  const_iterator end() const noexcept { return data_.GetAddress() + size_; }
  const_iterator cbegin() const noexcept { return data_.GetAddress(); }
  const_iterator cend() const noexcept { return data_.GetAddress() + size_; }

  Vector() = default;

  explicit Vector(size_t size) : data_(size), size_(size) {
    std::uninitialized_value_construct_n(data_.GetAddress(), size);
  }

  Vector(const Vector &other) : data_(other.size_), size_(other.size_) {
    std::uninitialized_copy_n(other.data_.GetAddress(), size_,
                              data_.GetAddress());
  }

  void Swap(Vector &other) noexcept {
    data_.Swap(other.data_);
    std::swap(size_, other.size_);
  }

  Vector(Vector &&other) noexcept { Swap(other); }

  Vector &operator=(const Vector &rhs) {
    if (this != &rhs) {
      if (rhs.size_ > data_.Capacity()) {
        Vector tmp(rhs);
        Swap(tmp);
      } else {
        /* Скопировать элементы из rhs, создав при необходимости новые
           или удалив существующие */
        size_t size_min = std::min(rhs.size_, size_); 
        std::copy(
          rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_min,
          data_.GetAddress()
        );
        if (rhs.size_ < size_)
          std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        else
          std::uninitialized_copy_n(
            rhs.data_.GetAddress() + size_,
            rhs.size_ - size_,
            data_.GetAddress() + size_
          );
      }
    }
    return *this;
  }

  Vector &operator=(Vector &&rhs) noexcept {
    Swap(rhs);
    return *this;
  }

  void Reserve(size_t new_capacity) {
    if (new_capacity <= capacity_) {
      return;
    }
    RawMemory<T> new_data(new_capacity);
    if constexpr (std::is_nothrow_move_constructible_v<T> ||
                  !std::is_copy_constructible_v<T>) {
      std::uninitialized_move_n(data_.GetAddress(), size_,
                                new_data.GetAddress());
    } else {
      std::uninitialized_copy_n(data_.GetAddress(), size_,
                                new_data.GetAddress());
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(new_data);
  }

  ~Vector() { std::destroy_n(data_.GetAddress(), size_); }

  size_t Size() const noexcept { return size_; }

  size_t Capacity() const noexcept { return data_.Capacity(); }

  const T &operator[](size_t index) const noexcept {
    return const_cast<Vector &>(*this)[index];
  }

  T &operator[](size_t index) noexcept {
    assert(index < size_);
    return data_[index];
  }

  void Resize(size_t size_new) {
    if (size_new < size_) {
      std::destroy_n(data_.GetAddress() + size_new, size_ - size_new);
    } else {
      Reserve(size_new);
      std::uninitialized_value_construct_n(data_.GetAddress() + size_,
                                           size_new - size_);
    }
    size_ = size_new;
  }

  template <typename V> void PushBack(V &&value) {
    EmplaceBack(std::forward<V>(value));
  }

  void PopBack() {
    if (0 < size_) {
      std::destroy_n(data_.GetAddress() + size_ - 1, 1);
      size_--;
    }
  }

  template <typename... Ts> T &EmplaceBack(Ts &&...vs) {
    return *Emplace(end(), std::forward<Ts>(vs)...);
  }


  template <typename... Args>
  iterator ReallocationEmplace(const_iterator pos, Args &&...args) {
    size_t idx = std::distance(cbegin(), pos);
    RawMemory<T> tmp(size_ == 0 ? 1 : size_ * 2);
    new (tmp.GetAddress() + idx) T(std::forward<Args>(args)...);
    if constexpr (std::is_nothrow_move_constructible_v<T> ||
                  !std::is_copy_constructible_v<T>) {
      std::uninitialized_move_n(begin(), std::distance(cbegin(), pos),
                                tmp.GetAddress());
      std::uninitialized_move_n(data_ + idx, size_ - idx,
                                tmp.GetAddress() + idx + 1);
    } else {
      std::uninitialized_copy_n(begin(), std::distance(cbegin(), pos),
                                tmp.GetAddress());
      std::uninitialized_copy_n(data_ + idx, size_ - idx,
                                tmp.GetAddress() + idx + 1);
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(tmp);
    ++size_;
    return data_.GetAddress() + idx;
  }

  template <typename... Args>
  iterator NonReallocationEmplace(const_iterator pos, Args &&...args) {
    size_t idx = std::distance(cbegin(), pos);
    if (pos == end()) {
      new (data_.GetAddress() + idx) T(std::forward<Args>(args)...);
    } else {
      T tmp(std::forward<Args>(args)...);
      new (data_.GetAddress() + size_) T(std::move(data_[size_ - 1]));
      std::move_backward(data_ + idx, end() - 1, end());
      data_[idx] = std::move(tmp);
    }
    ++size_;
    return data_.GetAddress() + idx;
  }

  template <typename... Args>
  iterator Emplace(const_iterator pos, Args &&...args) {
    size_t idx = std::distance(cbegin(), pos);
    if (size_ == Capacity()) {
      return ReallocationEmplace(pos, std::forward<Args>(args)...);
    } else {
      return NonReallocationEmplace(pos, std::forward<Args>(args)...);
    }
  }

  iterator Insert(const_iterator pos, const T &value) {
    return Emplace(pos, value);
  }

  iterator Insert(const_iterator pos, T &&value) {
    return Emplace(pos, std::move(value));
  }

  iterator Erase(const_iterator pos) {
    size_t idx = std::distance(cbegin(), pos);
    if constexpr (std::is_nothrow_move_constructible_v<T> ||
                  !std::is_copy_constructible_v<T>) {
      std::move(begin() + idx + 1, end(), begin() + idx);
    } else {
      std::copy(begin() + idx + 1, end(), begin() + idx);
    }
    std::destroy_n(end() - 1, 1);
    --size_;

    return data_.GetAddress() + idx;
  }

private:
  RawMemory<T> data_;
  size_t capacity_ = 0;
  size_t size_ = 0;

  // Выделяет сырую память под n элементов и возвращает указатель на неё
  static T *Allocate(size_t n) {
    return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
  }

  // Освобождает сырую память, выделенную ранее по адресу buf при помощи
  // Allocate
  static void Deallocate(T *buf) noexcept { operator delete(buf); }

  // Вызывает деструкторы n объектов массива по адресу buf
  static void DestroyN(T *buf, size_t n) noexcept {
    for (size_t i = 0; i != n; ++i) {
      Destroy(buf + i);
    }
  }

  // Создаёт копию объекта elem в сырой памяти по адресу buf
  static void CopyConstruct(T *buf, const T &elem) { new (buf) T(elem); }

  // Вызывает деструктор объекта по адресу buf
  static void Destroy(T *buf) noexcept { buf->~T(); }
};