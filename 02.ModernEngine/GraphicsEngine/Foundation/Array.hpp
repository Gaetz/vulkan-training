#pragma once

#include "Memory.hpp"
#include "Assert.hpp"


// Data structures //

// ArrayAligned //
template <typename T>
struct Array {

    Array();
    ~Array();

    void Init(Allocator* allocator, u32 initialCapacity, u32 initialSize = 0);
    void Shutdown();

    void Push(const T& element);

    // Grow the size and return T to be filled.
    T& PushUse();                 

    void Pop();
    void DeleteSwap(u32 index);

    T& operator[](u32 index);
    const T& operator[](u32 index) const;

    void Clear();
    void SetSize(u32 newSize);
    void SetCapacity(u32 newCapacity);
    void Grow(u32 newCapacity);

    T& Back();
    const T& Back() const;

    T& Front();
    const T& Front() const;

    u32 SizeInBytes() const;
    u32 CapacityInBytes() const;


    T* data;

    // Occupied size
    u32 size;    

    // Allocated capacity
    u32 capacity;

    Allocator* allocator;

}; // struct Array

// ArrayView //////////////////////////////////////////////////////////

// View over a contiguous memory block.
template <typename T>
struct ArrayView {

    ArrayView(T* data, u32 size);

    void Set(T* data, u32 size);

    T& operator[](u32 index);
    const T& operator[](u32 index) const;

    T* data;
    u32 size;
}; // struct ArrayView

// Implementation /////////////////////////////////////////////////////

// ArrayAligned ///////////////////////////////////////////////////////
template<typename T>
inline Array<T>::Array() {
    //GASSERT( true );
}

template<typename T>
inline Array<T>::~Array() {
    //GASSERT( data == nullptr );
}

template<typename T>
inline void Array<T>::Init(Allocator* allocator_, u32 initialCapacity, u32 initialSize) {
    data = nullptr;
    size = initialSize;
    capacity = 0;
    allocator = allocator_;

    if (initialCapacity > 0) {
        Grow(initialCapacity);
    }
}

template<typename T>
inline void Array<T>::Shutdown() {
    if (capacity > 0) {
        allocator->deallocate(data);
    }
    data = nullptr;
    size = capacity = 0;
}

template<typename T>
inline void Array<T>::Push(const T& element) {
    if (size >= capacity) {
        Grow(capacity + 1);
    }

    data[size++] = element;
}

template<typename T>
inline T& Array<T>::PushUse() {
    if (size >= capacity) {
        Grow(capacity + 1);
    }
    ++size;

    return Back();
}

template<typename T>
inline void Array<T>::Pop() {
    GASSERT(size > 0);
    --size;
}

template<typename T>
inline void Array<T>::DeleteSwap(u32 index) {
    GASSERT(size > 0 && index < size);
    data[index] = data[--size];
}

template<typename T>
inline T& Array<T>::operator [](u32 index) {
    GASSERT(index < size);
    return data[index];
}

template<typename T>
inline const T& Array<T>::operator [](u32 index) const {
    GASSERT(index < size);
    return data[index];
}

template<typename T>
inline void Array<T>::Clear() {
    size = 0;
}

template<typename T>
inline void Array<T>::SetSize(u32 newSize) {
    if (newSize > capacity) {
        Grow(newSize);
    }
    size = newSize;
}

template<typename T>
inline void Array<T>::SetCapacity(u32 newCapacity) {
    if (newCapacity > capacity) {
        Grow(newCapacity);
    }
}

template<typename T>
inline void Array<T>::Grow(u32 newCapacity) {
    // Double capacity everytime we need more space
    if (newCapacity < capacity * 2) {
        newCapacity = capacity * 2;
    }
    else if (newCapacity < 4) {
        newCapacity = 4;
    }

    // Allocate memory for new space and copy former data, then deallocate previous memory
    T* newData = (T*)allocator->Allocate(newCapacity * sizeof(T), alignof(T));
    if (capacity) {
        MemoryCopy(newData, data, capacity * sizeof(T));

        allocator->Deallocate(data);
    }

    data = newData;
    capacity = newCapacity;
}

template<typename T>
inline T& Array<T>::Back() {
    GASSERT(size);
    return data[size - 1];
}

template<typename T>
inline const T& Array<T>::Back() const {
    GASSERT(size);
    return data[size - 1];
}

template<typename T>
inline T& Array<T>::Front() {
    GASSERT(size);
    return data[0];
}

template<typename T>
inline const T& Array<T>::Front() const {
    GASSERT(size);
    return data[0];
}

template<typename T>
inline u32 Array<T>::SizeInBytes() const {
    return size * sizeof(T);
}

template<typename T>
inline u32 Array<T>::CapacityInBytes() const {
    return capacity * sizeof(T);
}

// ArrayView //////////////////////////////////////////////////////////
template<typename T>
inline ArrayView<T>::ArrayView(T* data_, u32 size_)
    : data(data_), size(size_) {
}

template<typename T>
inline void ArrayView<T>::Set(T* data_, u32 size_) {
    data = data_;
    size = size_;
}

template<typename T>
inline T& ArrayView<T>::operator[](u32 index) {
    GASSERT(index < size);
    return data[index];
}

template<typename T>
inline const T& ArrayView<T>::operator[](u32 index) const {
    GASSERT(index < size);
    return data[index];
}

