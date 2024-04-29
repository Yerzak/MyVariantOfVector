#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            capacity_ = 0;
            Swap(rhs);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        /*Операция копирующего присваивания должна применять идиому copy-and-swap только при нехватке места в векторе-приёмнике.
Когда будете присваивать вектор меньшего размера, не забудьте удалить лишние элементы из вектора-приёмника.*/
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ < size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    //--------------------------------------------------------------
    void Resize(size_t new_size) {
        if (size_ > new_size) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else if (size_ < new_size) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }
    //---------------------------------------------------------------
    void PushBack(const T& value) {
        if (size_ < Capacity()) {
            new(data_ + size_) T(value);
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
    }
    //------------------------------------------------------------
    void PushBack(T&& value) {
        if (size_ < Capacity()) {
            new(data_ + size_) T(std::move(value));
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
    }
    //-------------------------------------------------------
    void PopBack() noexcept {
        if (size_ > 0) {
            DestroyN(data_.GetAddress() + size_ - 1, 1);
            --size_;
        }
    }
    //---------------------------------------------------------------
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        /*Когда работа над методом EmplaceBack завершится, его можно будет использовать для реализации обоих методов PushBack.
Чтобы передать параметры конструктору элементов вектора, используйте forwarding-ссылки и std::forward.*/
        T* ref = nullptr;
        if (size_ < Capacity()) {
            ref = new(data_ + size_) T(std::forward<Args>(args)...);
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            ref = new(new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return *ref;
    }
    //----------------------------------------------------------------    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    //------------------------------------------------------------------------
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t pos_length = pos - begin();
        if (size_ < Capacity()) {
            try {
                T temp(std::forward<Args>(args)...);
                std::uninitialized_move_n(end() - 1, 1, end());
                std::move_backward(begin() + pos_length, end() - 1, end());
                data_[pos_length] = std::move(temp);
            }
            catch (...) {
                throw;
            }
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + pos_length) T(std::forward<Args>(args)...);
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), pos_length, new_data.GetAddress());
                    std::uninitialized_move_n(data_.GetAddress() + pos_length, size_ - pos_length, new_data.GetAddress() + pos_length + 1);
                }
                else {
                    std::uninitialized_copy_n(data_.GetAddress(), pos_length, new_data.GetAddress());
                    std::uninitialized_copy_n(data_.GetAddress() + pos_length, size_ - pos_length, new_data.GetAddress() + pos_length + 1);
                }
            }
            catch (...) {
                DestroyN(new_data.GetAddress(), size_ + 1);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return begin() + pos_length;
    }
    //----------------------------------------------------------------------
    iterator Erase(const_iterator pos) {
        size_t pos_length = pos - begin();
        std::move(begin() + pos_length + 1, end(), begin() + pos_length);
        DestroyN(end() - 1, 1);
        --size_;
        return begin() + pos_length;
    }
    //-------------------------------------------------------------------------
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }


private:
    RawMemory<T> data_;
    size_t size_ = 0;

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};
