#pragma once
#include "../external/tlsf.h"


#include "Platform.hpp"
#include "Service.hpp"

#define G_IMGUI

// Memory Methods /////////////////////////////////////////////////////
void MemoryCopy(void* destination, void* source, sizet size);

//  Calculate aligned memory size.
sizet MemoryAlign(sizet size, sizet alignment);

// Memory Structs /////////////////////////////////////////////////////
//
//
struct MemoryStatistics {
    sizet allocatedBytes;
    sizet totalBytes;

    u32 allocationCount;

    void Add(sizet a) {
        if (a) {
            allocatedBytes += a;
            ++allocationCount;
        }
    }
}; // struct MemoryStatistics

//
//
struct Allocator {
    virtual ~Allocator() { }
    virtual void* Allocate(sizet size, sizet alignment) = 0;
    virtual void* Allocate(sizet size, sizet alignment, cstring file, i32 line) = 0;

    virtual void Deallocate(void* pointer) = 0;
}; // struct Allocator


//
//
struct HeapAllocator : public Allocator {

    ~HeapAllocator() override;

    void Init(sizet size);
    void Shutdown();

#if defined G_IMGUI
    void DebugUi();
#endif // G_IMGUI

    void* Allocate(sizet size, sizet alignment) override;
    void* Allocate(sizet size, sizet alignment, cstring file, i32 line) override;

    void Deallocate(void* pointer) override;

    void* tlsfHandle;
    void* memory;
    sizet allocatedSize = 0;
    sizet maxSize = 0;

}; // struct HeapAllocator

//
//
struct StackAllocator : public Allocator {

    void Init(sizet size);
    void Shutdown();

    void* Allocate(sizet size, sizet alignment) override;
    void* Allocate(sizet size, sizet alignment, cstring file, i32 line) override;

    void Deallocate(void* pointer) override;

    sizet GetMarker();
    void FreeMarker(sizet marker);

    void Clear();

    u8* memory = nullptr;
    sizet totalSize = 0;
    sizet allocatedSize = 0;

}; // struct StackAllocator

//
//
struct DoubleStackAllocator : public Allocator {

    void Init(sizet size);
    void Shutdown();

    void* Allocate(sizet size, sizet alignment) override;
    void* Allocate(sizet size, sizet alignment, cstring file, i32 line) override;
    void Deallocate(void* pointer) override;

    void* AllocateTop(sizet size, sizet alignment);
    void* AllocateBottom(sizet size, sizet alignment);

    void DeallocateTop(sizet size);
    void DeallocateBottom(sizet size);

    sizet GetTopMarker();
    sizet GetBottomMarker();

    void FreeTopMarker(sizet marker);
    void FreeBottomMarker(sizet marker);

    void ClearTop();
    void ClearBottom();

    u8* memory = nullptr;
    sizet totalSize = 0;
    sizet top = 0;
    sizet bottom = 0;

}; // struct DoubleStackAllocator

//
// Allocator that can only be reset.
//
struct LinearAllocator : public Allocator {

    ~LinearAllocator();

    void Init(sizet size);
    void Shutdown();

    void* Allocate(sizet size, sizet alignment) override;
    void* Allocate(sizet size, sizet alignment, cstring file, i32 line) override;

    void Deallocate(void* pointer) override;

    void Clear();

    u8* memory = nullptr;
    sizet totalSize = 0;
    sizet allocatedSize = 0;
}; // struct LinearAllocator

//
// DANGER: this should be used for NON runtime processes, like compilation of resources.
struct MallocAllocator : public Allocator {
    void* Allocate(sizet size, sizet alignment) override;
    void* Allocate(sizet size, sizet alignment, cstring file, i32 line) override;

    void Deallocate(void* pointer) override;
};

// Memory Service /////////////////////////////////////////////////////
// 
// 
struct MemoryServiceConfiguration {

    sizet maximumDynamicSize = 32 * 1024 * 1024;    // Defaults to max 32MB of dynamic memory.

}; // struct MemoryServiceConfiguration
//
//
struct MemoryService : public Service {

    G_DECLARE_SERVICE(MemoryService);

    void Init(void* configuration);
    void Shutdown();

#if defined G_IMGUI
    void ImguiDraw();
#endif // G_IMGUI

    // Frame allocator
    LinearAllocator scratchAllocator;
    HeapAllocator systemAllocator;

    //
    // Test allocators.
    void Test();

    static constexpr cstring kName = "memory_service";

}; // struct MemoryService

// Macro helpers //////////////////////////////////////////////////////
#define GAlloca(size, allocator)    ((allocator)->Allocate( size, 1, __FILE__, __LINE__ ))
#define GAllocaM(size, allocator)   ((u8*)(allocator)->Allocate( size, 1, __FILE__, __LINE__ ))
#define GAllocaT(type, allocator)   ((type*)(allocator)->Allocate( sizeof(type), 1, __FILE__, __LINE__ ))

#define GAllocaA(size, allocator, alignment)    ((allocator)->Allocate( size, alignment, __FILE__, __LINE__ ))

#define GFree(pointer, allocator) (allocator)->Deallocate(pointer)

#define GKilo(size)                 (size * 1024)
#define GMega(size)                 (size * 1024 * 1024)
#define GGiga(size)                 (size * 1024 * 1024 * 1024)