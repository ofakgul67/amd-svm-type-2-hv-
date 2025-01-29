#pragma once
#include "../includes.h"

namespace svm {
    // Shadow structure for SVM
    struct alignas(16) shadow {
        struct {
            bool should_exit : 1;
            bool hide_overhead : 1;
        };

        svm::efer_msr efer;
        svm::hsave_pa_msr hsave_pa;
    };

    // Shared VCPU data structure
    struct alignas(0x1000) shared_vcpu {
        npt_data_t npt;                // Nested Page Table (NPT) data associated with the VCPU
        msrpm_t msrpm;                 // Map of MSR permissions for the VCPU
    };
    using pshared_vcpu_t = shared_vcpu*;   // Pointer type for shared_vcpu structure

    // VProcessor (virtual processor) data structure
    struct alignas(0x1000) vprocessor_data {
        union {
            // Host stack memory layout
            u8 host_stack_limit[0x6000];  // Host stack limit, used to manage the stack's memory
            struct {
                u8 stack_contents[0x6000 - (sizeof(u64) * 6) - sizeof(KTRAP_FRAME) - sizeof(shadow)];
                // Stack contents, minus space for critical structures

                u64 guest_vmcb_pa;      // Guest Virtual Machine Control Block (VMCB) physical address
                u64 host_vmcb_pa;       // Host VMCB physical address

                vprocessor_data* self;  // Pointer to the current VCPU data structure (self-reference)
                pshared_vcpu_t shared_vcpu;  // Pointer to the shared VCPU data

                u64 suppress_nrip_increment; // Used to suppress the next instruction pointer increment on certain VM exits
                u64 reserved1;               // Reserved for future use or padding

                KTRAP_FRAME trap_frame;      // Trap frame for saving CPU state during exceptions or interrupts

                shadow shadow_data;          // Shadow structure for SVM, contains state for MSR shadowing and exit control
            } host_stack_layout;
        };

        // Nested Page Table (NPT) and MSR permissions
        npt_data_t npt[2];                // Nested Page Table (NPT) data associated with the VCPU
        msrpm_t msrpm;                 // Map of MSR permissions for the VCPU

        // Guest and host VM control blocks (VMCB)
        vmcb_t guest_vmcb;      // VMCB for the guest VM (stores guest state)
        vmcb_t host_vmcb;       // VMCB for the host (stores host state)

        // Host state area
        u8 host_state_area[0x1000];   // Area to store host-specific state during VM exits
    };
    using pvcpu_t = vprocessor_data*;    // Pointer type for vprocessor_data structure

    // Ensure the size matches the kernel stack size and additional pages
    static_assert(sizeof(vprocessor_data) == 0x6000 + sizeof(npt_data_t) * 2 + sizeof(msrpm_t) + 0x1000 * 3,
        "vprocessor_data size mismatch");   // Ensure that vprocessor_data matches the expected size
}
