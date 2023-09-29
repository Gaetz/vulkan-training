#pragma once

#include "Platform.hpp"
#include "Memory.hpp"
#include "Assert.hpp"
#include "Array.hpp"

// Defines:
// G_BLOB_WRITE         - use it in code that can write blueprints,
//                            like data compilers.
#define G_BLOB_WRITE

struct Allocator;

//
//
template <typename T>
struct RelativePointer {

    T* Get() const;

    bool IsEqual(const RelativePointer& other) const;
    bool IsNull() const;
    bool IsNotNull() const;

    // Operator overloading to give a cleaner interface
    T* operator->() const;
    T& operator*() const;

#if defined G_BLOB_WRITE
    void Set(char* raw_pointer);
    void SetNull();
#endif // G_BLOB_WRITE

    i32 offset;

}; // struct RelativePointer


// RelativeArray //////////////////////////////////////////////////////////

//
//
template <typename T>
struct RelativeArray {

    const T& operator[](u32 index) const;
    T& operator[](u32 index);

    const T* Get() const;
    T* Get();

#if defined G_BLOB_WRITE
    void Set(char* raw_pointer, u32 size);
    void SetEmpty();
#endif // G_BLOB_WRITE

    u32 size;
    RelativePointer<T> data;
}; // struct RelativeArray


// RelativeString /////////////////////////////////////////////////////////

//
//
struct RelativeString : public RelativeArray<char> {

    cstring c_str() const { return data.Get(); }

    void Set(char* pointer_, u32 size_) { RelativeArray<char>::Set(pointer_, size_); }
}; // struct RelativeString



// Implementations/////////////////////////////////////////////////////////

// RelativePointer ////////////////////////////////////////////////////////
template<typename T>
inline T* RelativePointer<T>::Get() const {
    char* address = ((char*)&offset) + offset;
    return offset != 0 ? (T*)address : nullptr;
}

template<typename T>
inline bool RelativePointer<T>::IsEqual(const RelativePointer& other) const {
    return Get() == other.Get();
}

template<typename T>
inline bool RelativePointer<T>::IsNull() const {
    return offset == 0;
}

template<typename T>
inline bool RelativePointer<T>::IsNotNull() const {
    return offset != 0;
}

template<typename T>
inline T* RelativePointer<T>::operator->() const {
    return Get();
}

template<typename T>
inline T& RelativePointer<T>::operator*() const {
    return *(Get());
}

#if defined G_BLOB_WRITE
/* // TODO: useful or not ?
template<typename T>
inline void RelativePointer<T>::set( T* pointer ) {
    offset = pointer ? ( i32 )( ( ( char* )pointer ) - ( char* )this ) : 0;
}*/

template<typename T>
inline void RelativePointer<T>::Set(char* raw_pointer) {
    offset = raw_pointer ? (i32)(raw_pointer - (char*)this) : 0;
}
template<typename T>
inline void RelativePointer<T>::SetNull() {
    offset = 0;
}
#endif // G_BLOB_WRITE

// RelativeArray //////////////////////////////////////////////////////////
template<typename T>
inline const T& RelativeArray<T>::operator[](u32 index) const {
    GASSERT(index < size);
    return data.Get()[index];
}

template<typename T>
inline T& RelativeArray<T>::operator[](u32 index) {
    GASSERT(index < size);
    return data.Get()[index];
}

template<typename T>
inline const T* RelativeArray<T>::Get() const {
    return data.Get();
}

template<typename T>
inline T* RelativeArray<T>::Get() {
    return data.Get();
}

#if defined G_BLOB_WRITE
template<typename T>
inline void RelativeArray<T>::Set(char* rawPointer, u32 size_) {
    data.Set(rawPointer);
    size = size_;
}
template<typename T>
inline void RelativeArray<T>::SetEmpty() {
    size = 0;
    data.SetNull();
}
#endif

