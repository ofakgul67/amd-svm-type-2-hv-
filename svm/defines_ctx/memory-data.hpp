#pragma once
#include "../includes.h"

// How much of physical memory to map into the host address-space
inline constexpr u64 host_physical_memory_pd_count = 64;

// Physical Memory is directly mapped to this pml4 entry
inline constexpr u64 host_physical_memory_pml4_idx = 255;

// Directly access physical memory by using [base + offset]
inline u8* const host_physical_memory_base = reinterpret_cast<u8*>(
    host_physical_memory_pml4_idx << (9 + 9 + 9 + 12));

// Memory-related types and structures
typedef union _pml4e
{
    union {
        u64 value;
        struct {
            u64 present : 1;
            u64 write : 1;
            u64 usermode : 1;
            u64 page_write_thru : 1;
            u64 page_cache_disable : 1;
            u64 accessed : 1;
            u64 ignored6 : 1;
            u64 reserved7 : 2; // 0
            u64 available_to_software : 3;
            u64 pfn : 40;
            u64 available : 11;
            u64 no_execute : 1;
        };
    };
} pml4e, * ppml4e;

typedef union _pdpte
{
    union {
        u64 value;
        struct {
            u64 present : 1;
            u64 write : 1;
            u64 usermode : 1;
            u64 page_write_thru : 1;
            u64 page_cache_disable : 1;
            u64 accessed : 1;
            u64 dirty : 1; //only if huge
            u64 huge_page : 1;
            u64 global : 1; //only if huge
            u64 available_to_software : 3;
            u64 pfn : 40;
            u64 available : 11;
            u64 no_execute : 1;
        };
    };
} pdpte, * ppdpte;

typedef union _pde
{
    union {
        u64 value;
        struct {
            u64 present : 1;
            u64 write : 1;
            u64 usermode : 1;
            u64 page_write_thru : 1;
            u64 page_cache_disable : 1;
            u64 accessed : 1;
            u64 dirty : 1; // only if huge
            u64 large_page : 1;
            u64 global : 1; // only if huge
            u64 available_to_software : 3;
            u64 pfn : 40;
            u64 available : 11;
            u64 no_execute : 1;
        };
    };
} pde, * ppde;

typedef union _pte
{
    union {
        u64 value;
        struct {
            u64 present : 1;
            u64 write : 1;
            u64 usermode : 1;
            u64 page_write_thru : 1;
            u64 page_cache_disable : 1;
            u64 accessed : 1;
            u64 dirty : 1;
            u64 pat : 1;
            u64 global : 1; // 0
            u64 available_to_software : 3;
            u64 pfn : 40;
            u64 available : 7;
            u64 mpk : 4;
            u64 no_execute : 1;
        };
    };
} pte, * ppte;

typedef struct _pdpte_1gb_64
{
    union {
        u64 value;
        struct {
            u64 present : 1;
            u64 write : 1;
            u64 usermode : 1;
            u64 page_write_thru : 1;
            u64 page_cache_disable : 1;
            u64 accessed : 1;
            u64 dirty : 1; //only if huge
            u64 huge_page : 1;
            u64 global : 1; //only if huge
            u64 available_to_software : 3;
            u64 pat : 1;
            u64 reserved : 17;
            u64 pfn : 22;
            u64 available : 7;
            u64 mpk : 4;
            u64 no_execute : 1;
        };
    };
} pdpte_1gb_64, * ppdpte_1gb_64;

typedef union _pde_2mb_64
{
    u64 value;
    struct {
        u64 present : 1;
        u64 write : 1;
        u64 usermode : 1;
        u64 page_write_thru : 1;
        u64 page_cache_disable : 1;
        u64 accessed : 1;
        u64 dirty : 1; // only if huge
        u64 large_page : 1;
        u64 global : 1; // only if huge
        u64 available_to_software : 3;
        u64 pat : 1;
        u64 reserved : 8;
        u64 pfn : 31;
        u64 available : 7;
        u64 mpk : 4;
        u64 no_execute : 1;
    };
} pde_2mb_64, * ppde_2mb_64;

struct host_pt_t {
    // Array of PML4 entries that point to a PDPT
    alignas(0x1000) pml4e pml4[512];

    // PDPT for mapping physical memory
    alignas(0x1000) pdpte pdpt[512];

    // PDs for mapping physical memory
    alignas(0x1000) pde_2mb_64 pd[host_physical_memory_pd_count][512];
};