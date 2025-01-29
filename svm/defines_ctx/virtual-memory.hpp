#pragma once
#include "../includes.h"

typedef union _virt_addr_t
{
    u64 value;
    struct
    {
        u64 offset_4kb : 12;
        u64 pt_index : 9;
        u64 pd_index : 9;
        u64 pdpt_index : 9;
        u64 pml4_index : 9;
        u64 reserved : 16;
    };

    struct
    {
        u64 offset_2mb : 21;
        u64 pd_index : 9;
        u64 pdpt_index : 9;
        u64 pml4_index : 9;
        u64 reserved : 16;
    };

    struct
    {
        u64 offset_1gb : 30;
        u64 pdpt_index : 9;
        u64 pml4_index : 9;
        u64 reserved : 16;
    };

} virt_addr_t, * pvirt_addr_t;
using phys_addr_t = virt_addr_t;