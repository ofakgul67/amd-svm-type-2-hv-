#pragma once
#include "../includes.h"

class npt {
public:
    enum nested_page_table : u64 { primary, shadow };

    // Core NPT management functions
    auto prepare_npt(npt_data_t& npt) -> svm::svmroot_error_t;
    auto split_npt_pde(npt_data_t& npt, pde_2mb_64* const pde_2mb) -> void;
    auto get_npt_pte(npt_data_t& npt, u64 const physical_address, bool const force_split) -> pte*;

    // Hook management
    auto find_npt_hook(npt_data_t& npt_data, u64 page_pfn) -> npt_hook_node_t*;
    auto install_npt_hook(
        svm::pvcpu_t vcpu,
        npt_data_t& primary_npt,
        npt_data_t& shadow_npt,
        u64 original_page_pfn,
        u64 executable_page_pfn
    ) -> svm::svmroot_error_t;
    auto remove_npt_hook(
        svm::pvcpu_t vcpu,
        npt_data_t& npt,
        u64 original_page_pfn
    ) -> svm::svmroot_error_t;

    auto cleanup() -> void;
    static auto get() -> npt*;

private:
    KSPIN_LOCK npt_lock;

    // Helper functions
    void acquire_lock(PKIRQL old_irql);
    void release_lock(KIRQL old_irql);
    void setup_primary_pte(pte* pte, u64 pfn);
    void setup_shadow_pte(pte* pte, u64 pfn);
    void mark_vmcb_modified(svm::pvcpu_t vcpu);
    void flush_tlb_safe(svm::pvcpu_t vcpu);
    bool update_hook_list(npt_data_t& npt, u64 orig_pfn, u64 exec_pfn);
};