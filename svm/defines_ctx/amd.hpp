#pragma once
#include "../includes.h"

namespace svm {
    namespace globals {
        struct {
            u64 kprocess_directory_table_base;
            u64 eprocess_unique_process_id;
            u64 eprocess_image_file_name;
            u64 kpcr_pcrb;
            u64 kprcb_current_thread;
            u64 kthread_apc_state;
            u64 kapc_state_process;
        } inline offsets;

        // Public members 
        inline u64 core_count;

        // Globals 
        inline _EPROCESS* system_eprocess;
        inline cr3_t system_cr3;
        inline u32 windows_build_number;
    };

    typedef struct _processor_number {
        u16 group;
        u8 number;
        u8 reserved;

        ULONG get_processor_index() {
            PROCESSOR_NUMBER processor_num;
            KeGetCurrentProcessorNumberEx(&processor_num);
            return KeGetProcessorIndexFromNumber(&processor_num);
        }

    } processor_number, * pprocessor_number;

    typedef struct DECLSPEC_ALIGN(16) DECLSPEC_NOINITALL _context_t {
        u64 p1_home;
        u64 p2_home;
        u64 p3_home;
        u64 p4_home;
        u64 p5_home;
        u64 p6_home;
        u32 context_flags;
        u32 mx_csr;
        u16 seg_cs;
        u16 seg_ds;
        u16 seg_es;
        u16 seg_fs;
        u16 seg_gs;
        u16 seg_ss;
        u32 e_flags;
        u64 dr0;
        u64 dr1;
        u64 dr2;
        u64 dr3;
        u64 dr6;
        u64 dr7;
        u64 rax;
        u64 rcx;
        u64 rdx;
        u64 rbx;
        u64 rsp;
        u64 rbp;
        u64 rsi;
        u64 rdi;
        u64 r8;
        u64 r9;
        u64 r10;
        u64 r11;
        u64 r12;
        u64 r13;
        u64 r14;
        u64 r15;
        u64 rip;
        union {
            XMM_SAVE_AREA32 flt_save;
            struct {
                M128A header[2];
                M128A legacy[8];
                M128A xmm0;
                M128A xmm1;
                M128A xmm2;
                M128A xmm3;
                M128A xmm4;
                M128A xmm5;
                M128A xmm6;
                M128A xmm7;
                M128A xmm8;
                M128A xmm9;
                M128A xmm10;
                M128A xmm11;
                M128A xmm12;
                M128A xmm13;
                M128A xmm14;
                M128A xmm15;
            } dummy_struct_name;
        } dummy_union_name;
        M128A vector_register[26];
        u64 vector_control;
        u64 debug_control;
        u64 last_branch_to_rip;
        u64 last_branch_from_rip;
        u64 last_exception_to_rip;
        u64 last_exception_from_rip;

        void* get_current_context() {
            pcontext_t current_ctx = reinterpret_cast<pcontext_t>(
                ExAllocatePoolWithTag(NonPagedPool, sizeof(svm::context_t), 'vmm'));
            if (current_ctx) {
                RtlZeroMemory(current_ctx, sizeof(svm::context_t));
                RtlCaptureContext(reinterpret_cast<PCONTEXT>(current_ctx));
                return current_ctx;
            }

            return nullptr;
        }
    } context_t, * pcontext_t;
}

class hypervisor {
private:
    hypervisor() = default;
    void initialize();

public:
    // Operators
    void* operator new(size_t size) {
        return ExAllocatePoolWithTag(NonPagedPool, size, '1000');
    }
    void operator delete(void* p) noexcept {
        ExFreePool(p);
    }
    void* operator new[](size_t size) {
        return ExAllocatePoolWithTag(NonPagedPool, size, '1000');
    }
    void operator delete[](void* p) noexcept {
        ExFreePool(p);
    }
    void operator delete(void* p, size_t) noexcept {
        ExFreePool(p);
    }
    void operator delete[](void* p, size_t) noexcept {
        ExFreePool(p);
    }

    // VCPUs
    svm::pvcpu_t vcpus[32];

    // Host page tables that are shared between vcpus
    host_pt_t shared_host_pt;

    // Public methods
    bool is_core_virtualized(u8 core_number);
    void cleanup_on_exit();

    static hypervisor* get();
};
