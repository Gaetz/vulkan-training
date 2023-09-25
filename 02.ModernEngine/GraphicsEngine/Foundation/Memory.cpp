#include "Memory.hpp"

#include "memory.hpp"
#include "assert.hpp"

#include "../external/tlsf.h"

#include <stdlib.h>
#include <memory.h>

#if defined G_IMGUI
#include "../external/imgui/imgui.h"
#endif // G_IMGUI

// Define this and add StackWalker to heavy memory profile
//#define G_MEMORY_STACK

//
#define HEAP_ALLOCATOR_STATS

#if defined (G_MEMORY_STACK)
#include "StackWalker.h"
#endif // G_MEMORY_STACK


//#define G_MEMORY_DEBUG
#if defined (G_MEMORY_DEBUG)
#define hy_mem_assert(cond) hy_assert(cond)
#else
#define hy_mem_assert(cond)
#endif // G_MEMORY_DEBUG

// Memory Service /////////////////////////////////////////////////////////
static MemoryService    sMemoryService;

// Locals
static sizet sSize = RMega(32) + tlsf_size() + 8;

//
// Walker methods
static void ExitWalker(void* ptr, size_t size, int used, void* user);
static void ImguiWalker(void* ptr, size_t size, int used, void* user);

MemoryService* MemoryService::Instance() {
    return &sMemoryService;
}

//
//
void MemoryService::Init(void* configuration) {

    GPrint("Memory Service Init\n");
    MemoryServiceConfiguration* memoryConfiguration = static_cast<MemoryServiceConfiguration*>(configuration);
    systemAllocator.Init(memoryConfiguration ? memoryConfiguration->maximumDynamicSize : sSize);
}

void MemoryService::Shutdown() {

    systemAllocator.Shutdown();

    GPrint("Memory Service Shutdown\n");
}

void ExitWalker(void* ptr, size_t size, int used, void* user) {
    MemoryStatistics* stats = (MemoryStatistics*)user;
    stats->Add(used ? size : 0);

    if (used)
        GPrint("Found active allocation %p, %llu\n", ptr, size);
}

#if defined G_IMGUI
void ImguiWalker(void* ptr, size_t size, int used, void* user) {

    u32 memorySize = (u32)size;
    cstring memory_unit = "b";
    if (memorySize > 1024 * 1024) {
        memorySize /= 1024 * 1024;
        memory_unit = "Mb";
    }
    else if (memorySize > 1024) {
        memorySize /= 1024;
        memory_unit = "kb";
    }
    ImGui::Text("\t%p %s size: %4llu %s\n", ptr, used ? "used" : "free", memorySize, memory_unit);

    MemoryStatistics* stats = (MemoryStatistics*)user;
    stats->Add(used ? size : 0);
}


void MemoryService::ImguiDraw() {

    if (ImGui::Begin("Memory Service")) {

        systemAllocator.DebugUi();
    }
    ImGui::End();
}
#endif // G_IMGUI

void MemoryService::Test() {

    //static u8 mem[ 1024 ];
    //LinearAllocator la;
    //la.Init( mem, 1024 );

    //// Allocate 3 times
    //void* a1 = ralloca( 16, &la );
    //void* a2 = ralloca( 20, &la );
    //void* a4 = ralloca( 10, &la );
    //// Free based on size
    //la.free( 10 );
    //void* a3 = ralloca( 10, &la );
    //GASSERT( a3 == a4 );

    //// Free based on pointer
    //rfree( a2, &la );
    //void* a32 = ralloca( 10, &la );
    //GASSERT( a32 == a2 );
    //// Test out of bounds 
    //u8* out_bounds = ( u8* )a1 + 10000;
    //rfree( out_bounds, &la );
}

// Memory Structs /////////////////////////////////////////////////////////

// HeapAllocator //////////////////////////////////////////////////////////
HeapAllocator::~HeapAllocator() {
}

void HeapAllocator::Init(sizet size) {
    // Allocate
    memory = malloc(size);
    maxSize = size;
    allocatedSize = 0;

    tlsfHandle = tlsf_create_with_pool(memory, size);

    GPrint("HeapAllocator of size %llu created\n", size);
}

void HeapAllocator::Shutdown() {

    // Check memory at the application exit.
    MemoryStatistics stats{ 0, maxSize };
    pool_t pool = tlsf_get_pool(tlsfHandle);
    tlsf_walk_pool(pool, ExitWalker, (void*)&stats);

    if (stats.allocatedBytes) {
        GPrint("HeapAllocator Shutdown.\n===============\nFAILURE! Allocated memory detected. allocated %llu, total %llu\n===============\n\n", stats.allocatedBytes, stats.totalBytes);
    }
    else {
        GPrint("HeapAllocator Shutdown - all memory free!\n");
    }

    GASSERTM(stats.allocatedBytes == 0, "Allocations still present. Check your code!");

    tlsf_destroy(tlsfHandle);

    free(memory);
}

#if defined G_IMGUI
void HeapAllocator::DebugUi() {

    ImGui::Separator();
    ImGui::Text("Heap Allocator");
    ImGui::Separator();
    MemoryStatistics stats{ 0, maxSize };
    pool_t pool = tlsf_get_pool(tlsfHandle);
    tlsf_walk_pool(pool, ImguiWalker, (void*)&stats);

    ImGui::Separator();
    ImGui::Text("\tAllocation count %d", stats.allocationCount);
    ImGui::Text("\tAllocated %llu K, free %llu Mb, total %llu Mb", stats.allocatedBytes / (1024 * 1024), (maxSize - stats.allocatedBytes) / (1024 * 1024), maxSize / (1024 * 1024));
}
#endif // G_IMGUI


#if defined (G_MEMORY_STACK)
class GStackWalker : public StackWalker {
public:
    GStackWalker() : StackWalker() {}
protected:
    virtual void OnOutput(LPCSTR szText) {
        GPrint("\nStack: \n%s\n", szText);
        StackWalker::OnOutput(szText);
    }
}; // class GStackWalker

void* HeapAllocator::Allocate(sizet size, sizet alignment) {

    /*if ( size == 16 )
    {
        GStackWalker sw;
        sw.ShowCallstack();
    }*/

    void* mem = tlsf_malloc(tlsfHandle, size);
    GPrint("Mem: %p, size %llu \n", mem, size);
    return mem;
}
#else

void* HeapAllocator::Allocate(sizet size, sizet alignment) {
#if defined (HEAP_ALLOCATOR_STATS)
    void* allocatedMemory = alignment == 1 ? tlsf_malloc(tlsfHandle, size) : tlsf_memalign(tlsfHandle, alignment, size);
    sizet actual_size = tlsf_block_size(allocatedMemory);
    allocatedSize += actual_size;

    /*if ( size == 52224 ) {
        return allocatedMemory;
    }*/
    return allocatedMemory;
#else
    return tlsf_malloc(tlsfHandle, size);
#endif // HEAP_ALLOCATOR_STATS
}
#endif // G_MEMORY_STACK

void* HeapAllocator::Allocate(sizet size, sizet alignment, cstring file, i32 line) {
    return Allocate(size, alignment);
}

void HeapAllocator::Deallocate(void* pointer) {
#if defined (HEAP_ALLOCATOR_STATS)
    sizet actualSize = tlsf_block_size(pointer);
    allocatedSize -= actualSize;

    tlsf_free(tlsfHandle, pointer);
#else
    tlsf_free(tlsfHandle, pointer);
#endif
}

// LinearAllocator /////////////////////////////////////////////////////////

LinearAllocator::~LinearAllocator() {
}

void LinearAllocator::Init(sizet size) {

    memory = (u8*)malloc(size);
    totalSize = size;
    allocatedSize = 0;
}

void LinearAllocator::Shutdown() {
    Clear();
    free(memory);
}

void* LinearAllocator::Allocate(sizet size, sizet alignment) {
    GASSERT(size > 0);

    const sizet newStart = MemoryAlign(allocatedSize, alignment);
    GASSERT(newStart < totalSize);
    const sizet newAllocatedSize = newStart + size;
    if (newAllocatedSize > totalSize) {
        hy_mem_assert(false && "Overflow");
        return nullptr;
    }

    allocatedSize = newAllocatedSize;
    return memory + newStart;
}

void* LinearAllocator::Allocate(sizet size, sizet alignment, cstring file, i32 line) {
    return Allocate(size, alignment);
}

void LinearAllocator::Deallocate(void*) {
    // This allocator does not allocate on a per-pointer base!
}

void LinearAllocator::Clear() {
    allocatedSize = 0;
}

// Memory Methods /////////////////////////////////////////////////////////
void MemoryCopy(void* destination, void* source, sizet size) {
    memcpy(destination, source, size);
}

sizet MemoryAlign(sizet size, sizet alignment) {
    const sizet alignmentMask = alignment - 1;
    return (size + alignmentMask) & ~alignmentMask;
}

// MallocAllocator ///////////////////////////////////////////////////////
void* MallocAllocator::Allocate(sizet size, sizet alignment) {
    return malloc(size);
}

void* MallocAllocator::Allocate(sizet size, sizet alignment, cstring file, i32 line) {
    return malloc(size);
}

void MallocAllocator::Deallocate(void* pointer) {
    free(pointer);
}

// StackAllocator ////////////////////////////////////////////////////////
void StackAllocator::Init(sizet size) {
    memory = (u8*)malloc(size);
    allocatedSize = 0;
    totalSize = size;
}

void StackAllocator::Shutdown() {
    free(memory);
}

void* StackAllocator::Allocate(sizet size, sizet alignment) {
    GASSERT(size > 0);

    const sizet newStart = MemoryAlign(allocatedSize, alignment);
    GASSERT(newStart < totalSize);
    const sizet new_allocated_size = newStart + size;
    if (new_allocated_size > totalSize) {
        hy_mem_assert(false && "Overflow");
        return nullptr;
    }

    allocatedSize = new_allocated_size;
    return memory + newStart;
}

void* StackAllocator::Allocate(sizet size, sizet alignment, cstring file, i32 line) {
    return Allocate(size, alignment);
}

void StackAllocator::Deallocate(void* pointer) {

    GASSERT(pointer >= memory);
    GASSERTM(pointer < memory + totalSize, "Out of bound free on linear allocator (outside bounds). Tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", (u8*)pointer, (u8*)pointer - memory, memory, totalSize, allocatedSize);
    GASSERTM(pointer < memory + allocatedSize, "Out of bound free on linear allocator (inside bounds, after allocated). Tempting to free %p, %llu after beginning of buffer (memory %p size %llu, allocated %llu)", (u8*)pointer, (u8*)pointer - memory, memory, totalSize, allocatedSize);

    const sizet sizeAtPointer = (u8*)pointer - memory;

    allocatedSize = sizeAtPointer;
}

sizet StackAllocator::GetMarker() {
    return allocatedSize;
}

void StackAllocator::FreeMarker(sizet marker) {
    const sizet difference = marker - allocatedSize;
    if (difference > 0) {
        allocatedSize = marker;
    }
}

void StackAllocator::Clear() {
    allocatedSize = 0;
}

// DoubleStackAllocator //////////////////////////////////////////////////
void DoubleStackAllocator::Init(sizet size) {
    memory = (u8*)malloc(size);
    top = size;
    bottom = 0;
    totalSize = size;
}

void DoubleStackAllocator::Shutdown() {
    free(memory);
}

void* DoubleStackAllocator::Allocate(sizet size, sizet alignment) {
    GASSERT(false);
    return nullptr;
}

void* DoubleStackAllocator::Allocate(sizet size, sizet alignment, cstring file, i32 line) {
    GASSERT(false);
    return nullptr;
}

void DoubleStackAllocator::Deallocate(void* pointer) {
    GASSERT(false);
}

void* DoubleStackAllocator::AllocateTop(sizet size, sizet alignment) {
    GASSERT(size > 0);

    const sizet newStart = MemoryAlign(top - size, alignment);
    if (newStart <= bottom) {
        hy_mem_assert(false && "Overflow Crossing");
        return nullptr;
    }

    top = newStart;
    return memory + newStart;
}

void* DoubleStackAllocator::AllocateBottom(sizet size, sizet alignment) {
    GASSERT(size > 0);

    const sizet newStart = MemoryAlign(bottom, alignment);
    const sizet newAllocatedSize = newStart + size;
    if (newAllocatedSize >= top) {
        hy_mem_assert(false && "Overflow Crossing");
        return nullptr;
    }

    bottom = newAllocatedSize;
    return memory + newStart;
}

void DoubleStackAllocator::DeallocateTop(sizet size) {
    if (size > totalSize - top) {
        top = totalSize;
    }
    else {
        top += size;
    }
}

void DoubleStackAllocator::DeallocateBottom(sizet size) {
    if (size > bottom) {
        bottom = 0;
    }
    else {
        bottom -= size;
    }
}

sizet DoubleStackAllocator::GetTopMarker() {
    return top;
}

sizet DoubleStackAllocator::GetBottomMarker() {
    return bottom;
}

void DoubleStackAllocator::FreeTopMarker(sizet marker) {
    if (marker > top && marker < totalSize) {
        top = marker;
    }
}

void DoubleStackAllocator::FreeBottomMarker(sizet marker) {
    if (marker < bottom) {
        bottom = marker;
    }
}

void DoubleStackAllocator::ClearTop() {
    top = totalSize;
}

void DoubleStackAllocator::ClearBottom() {
    bottom = 0;
}

