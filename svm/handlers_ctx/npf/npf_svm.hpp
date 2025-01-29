#pragma once
#include "../includes.h"

class npf_handler {
private:
    KSPIN_LOCK npf_lock;

    // Handlers for different fault types
    bool handle_not_present_fault(svm::pvcpu_t vcpu, const page_context& ctx);
    bool handle_execute_fault(svm::pvcpu_t vcpu, const page_context& ctx);
    bool handle_write_fault(svm::pvcpu_t vcpu, const page_context& ctx);

    // Helper functions
    void acquire_lock(PKIRQL old_irql);
    void release_lock(KIRQL old_irql);
    void mark_vmcb_modified(svm::pvcpu_t vcpu);
    void flush_tlb_if_needed(svm::pvcpu_t vcpu);
    void setup_pte(pte* pte, u64 pfn, bool allow_exec, bool allow_write);

public:
    static npf_handler* get();
    void npf_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void initialize();
};
