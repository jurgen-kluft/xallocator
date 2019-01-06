# Virtual Memory Allocator

These are just thoughts on a virtual memory allocator

1. CPU Fixed-Size-Allocator, 8 <= SIZE <= 4 KB
   16 KB page size
2. Virtual Memory: Alloc, Cache and Free pages back to system

Let's say an APP has 640 GB of address space and it has the following behaviour:

1. Many small allocations (FSA Heap)
2. CPU and GPU allocations (different calls, different page settings, VirtualAlloc/XMemAlloc)
3. Categories of GPU resources have different min/max size, alignment requirements, count and frequency

## Fixed Size Allocator [Ok]

Not too hard to make multi-thread safe using atomics where the only hard multi-threading problem is page commit/decommit.

Page Size = 4 KB, 16 KB or 64 KB
FSA  = 512 MB Address Space / page size = 8192 pages  
Address space, BEGIN - END  

Smaller size range (4-64) can use 4 KB page size.

```c++
struct Page {
    enum {
        SIZE_GLOBAL_LIST = 0,
        SIZE_HASFREE_LIST = 1,
    }
    u16     m_freelist;
    u16     m_refcount;
    u16     m_prev;
    u16     m_next;
};

struct FSA
{
    enum {
        MIN_SIZE = 8,
        MAX_SIZE = 8 * 1024,
        INC_SIZE = 4,
        PAGE_SIZE = 16 * 1024,
        ADDR_RANGE = 512 * 1024 * 1024
    };
    Page        m_pages[ADDR_RANGE / PAGE_SIZE];
    u16         m_pages_freelist;
    u32         m_sizes_freelist[MAX_SIZE / INC_SIZE];

    VirtualMemory*  m_vmem;
};

```

- Tiny implementation [+]
- Very low wastage [+]
- Can make use of flexible memory [+]
- Fast [+]
- Difficult to detect memory corruption [-]

## Large Size Allocator [Ok]

Features:

- Sizes > 32 MB
- Reserves huge virtual address space (160GB)
- Each table divided into equal sized slots
- Maps and unmaps 64kB pages on demand
- Guarantees contiguous memory

Pros and Cons:

- No headers [+]
- Simple implementation (~200 lines of code) [+]
- No fragmentation [+]
- Size rounded up to page size [-]
- Mapping and unmapping kernel calls relatively slow [-]

## Medium Size Allocator [WIP]

- All other sizes go here (4 KB < Size < 32 MB)
- Non-contiguous virtual pages
- Grows and shrinks
- 512 GB address space
- Page = 2 MB
- Space = 32 MB (16 pages)
- Space is tracked with external bookkeeping
- Suitable for GPU memory

## Clear Values (1337 speak)

- memset to byte value
  - Keep it memorable
- 0xFA – Flexible memory Allocated
- 0xFF – Flexible memory Free
- 0xDA – Direct memory Allocated
- 0xDF – Direct memory Free
- 0xA1 – memory ALlocated
- 0xDE – memory DEallocated

## Allocation management for `Medium Size Allocator`

Address Table

- MemBlock = 32 MB
- Min-Alloc-Size = 8 KB
- Max-Alloc-Size < 32 MB
- MemBlock / Min-Alloc-Size = 4096
- Node = Prev/Next, 2 B + 2 B
- Total memory is 8 KB

Size Table

- MinSize = 8 KB
- MaxSize = 32 MB
- Granularity = 1 KB
- Num Entries = 32 K
- Entry is NodeIdx, 4 B

- Size-Array is 32 K * 4 B = 128 KB
- Bit tree = 8/64/512/4096/32768 = 8 KB
- Total Memory = 128 KB + 8 KB = 136 KB

You can have a `Medium Size Allocator` per thread under the condition that you keep the pointer/memory to that thread. If you need memory to pass around we can use a global 'Medium Size Allocator'.


## Notes

PS4 = 994 GB address space
<http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/MacDougall_Aaron_Building_A_Low.pdf>

*/