#pragma once
#include "../includes.h"

class hide_page {
private:
    KSPIN_LOCK hide_lock;
    void acquire_lock(PKIRQL old_irql);
    void release_lock(KIRQL old_irql);
    void mark_vmcb_modified(svm::pvcpu_t vcpu);
    void flush_tlb_safe(svm::pvcpu_t vcpu);

public:
    hide_page();
    static hide_page* get();
    auto hide_physical_page(svm::pvcpu_t vcpu, u64 page_pfn) -> svm::svmroot_error_t;
    auto unhide_physical_page(svm::pvcpu_t vcpu, u64 page_pfn) -> svm::svmroot_error_t;
};