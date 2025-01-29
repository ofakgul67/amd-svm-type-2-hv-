#pragma once
#include "../includes.h"
#pragma pack(push, 1)

namespace svm
{
    enum class attribute_type : uint64_t
    {
        uncacheable = 0,
        write_combining = 1,
        write_through = 4,
        write_protected = 5,
        write_back = 6,
        uncacheable_no_write_combinining = 7
    };

    typedef struct
    {
        uint16_t limit;
        uintptr_t base;
    } descriptor_table_register_t, * pdescriptor_table_register_t;
    static_assert(sizeof(descriptor_table_register_t) == 10,
        "descriptor_table_register size mismatch");

    typedef union efer_msr
    {
        struct
        {
            uint64_t syscall : 1;
            uint64_t reserved1 : 7;
            uint64_t long_mode_enable : 1;
            uint64_t reserved2 : 1;
            uint64_t long_mode_active : 1;
            uint64_t nx_page : 1;
            uint64_t svme : 1;
            uint64_t lmsle : 1;
            uint64_t ffxse : 1;
            uint64_t reserved3 : 1;
            uint64_t reserved4 : 47;
        };
        uint64_t value;
    };

    static_assert(sizeof(efer_msr) == 8, "EFER MSR Size Mismatch");

    typedef union hsave_pa_msr {
        struct { // a page aligned physical address
            uint64_t must_be_zero : 12;
            uint64_t host_save_area_pa : 52;
        };
        uint64_t value;
    };

    static_assert(sizeof(hsave_pa_msr) == 8, "HSAVE PA MSR Size Mismatch");

    typedef union pat_msr {
        struct {
            attribute_type pa0 : 3;
            uint64_t reserved3 : 5;
            attribute_type pa1 : 3;
            uint64_t reserved11 : 5;
            attribute_type pa2 : 3;
            uint64_t reserved19 : 5;
            attribute_type pa3 : 3;
            uint64_t reserved27 : 5;
            attribute_type pa4 : 3;
            uint64_t reserved35 : 5;
            attribute_type pa5 : 3;
            uint64_t reserved43 : 5;
            attribute_type pa6 : 3;
            uint64_t reserved51 : 5;
            attribute_type pa7 : 3;
            uint64_t reserved59 : 5;
        };
        uint64_t value;
    };

    static_assert(sizeof(pat_msr) == 8, "PAT MSR Size Mismatch");

    typedef struct
    {
        union
        {
            uint64_t value;
            struct
            {
                uint16_t limit_low;
                uint16_t base_low;
                uint32_t base_middle : 8;
                uint32_t type : 4;
                uint32_t system : 1;
                uint32_t dpl : 2;
                uint32_t present : 1;
                uint32_t limit_high : 4;
                uint32_t avl : 1;
                uint32_t long_mode : 1;
                uint32_t default_bit : 1;
                uint32_t granularity : 1;
                uint32_t base_high : 8;
            } fields;
        };
    } segment_descriptor_t, * psegment_descriptor_t;
    static_assert(sizeof(segment_descriptor_t) == 8,
        "segment_descriptor size mismatch");

    typedef struct
    {
        union
        {
            uint16_t value;
            struct
            {
                uint16_t type : 4;
                uint16_t system : 1;
                uint16_t dpl : 2;
                uint16_t present : 1;
                uint16_t avl : 1;
                uint16_t long_mode : 1;
                uint16_t default_bit : 1;
                uint16_t granularity : 1;
                uint16_t reserved1 : 4;
            } fields;
        };
    } segment_attributes_t, * psegment_attributes_t;
    static_assert(sizeof(segment_attributes_t) == 2,
        "segment_attributes size mismatch");

    typedef union
    {
        uint16_t value;
        struct {
            uint16_t rpl : 2;
            uint16_t table : 1;
            uint16_t index : 13;
        };
    } segment_selector_t, * psegment_selector_t;
    static_assert(sizeof(segment_selector_t) == 2,
        "segment_selector__t size mismatch");
}

#pragma pack (pop)