#pragma once
#include "../includes.h"

// number of PDs in the NPT paging structures
inline constexpr size_t npt_pd_count = 64;
inline constexpr size_t npt_free_page_count = 100;

// max number of MMRs
inline constexpr size_t npt_mmr_count = 100;

struct npt_hook_node_t {
    npt_hook_node_t* next;

    // these can be stored as 32-bit integers to conserve space since
    // nobody is going to have more than 16,000 GB of physical memory
    u32 orig_pfn;
    u32 exec_pfn;
};

// TODO: refactor this to just use an array instead of a linked list
struct npt_hooks_t {
    // buffer of nodes (there can be unused nodes in the middle
    // of the buffer if a hook was removed for example)
    static constexpr size_t capacity = 64;
    npt_hook_node_t buffer[capacity];

    // list of currently active NPT hooks
    npt_hook_node_t* active_list_head;

    // list of unused nodes
    npt_hook_node_t* free_list_head;
};

// TODO: make this a bitfield instead
enum mmr_memory_mode {
    mmr_memory_mode_r = 0b001,
    mmr_memory_mode_w = 0b010,
    mmr_memory_mode_x = 0b100
};

// monitored memory ranges
struct npt_mmr_entry_t {
    // start physical address
    u64 start;

    // size of the range in bytes, a value of 0 indicates that this entry isn't being used
    u32 size;

    // the memory access type that we are monitoring for
    u8 mode;
};

struct npt_data_t {
    // NPT PML4
    alignas(0x1000) pml4e pml4[512];

    // NPT PDPT - a single one covers 512GB of physical memory
    alignas(0x1000) pdpte pdpt[512];
    static_assert(npt_pd_count <= 512, "Only 512 NPT PDs are supported!");

    // an array of NPT PDs - each PD covers 1GB
    union {
        alignas(0x1000) pde     pds[npt_pd_count][512];
        alignas(0x1000) pde_2mb_64 pds_2mb[npt_pd_count][512];
    };

    // free pages that can be used to split PDEs or for other purposes
    alignas(0x1000) u8 free_pages[npt_free_page_count][0x1000];

    // a dummy page that hidden pages are pointed to
    alignas(0x1000) u8 dummy_page[0x1000];
    u64 dummy_page_pfn;

    // an array of PFNs that point to each free page in the free page array
    u64 free_page_pfns[npt_free_page_count];

    // # of free pages that are currently in use
    size_t num_used_free_pages;

    // NPT hooks
    npt_hooks_t hooks;

    // monitored memory ranges
    npt_mmr_entry_t mmr[npt_mmr_count];

    // PTE of the page that we should re-enable memory monitoring on
    pte* mmr_mtf_pte;
    u8  mmr_mtf_mode;
};