#include "hide_page.hpp"

hide_page::hide_page() {
    KeInitializeSpinLock(&hide_lock);
}

hide_page* hide_page::get() {
    static hide_page instance;
    return &instance;
}

void hide_page::acquire_lock(PKIRQL old_irql) {
    KeAcquireSpinLock(&hide_lock, old_irql);
}

void hide_page::release_lock(KIRQL old_irql) {
    KeReleaseSpinLock(&hide_lock, old_irql);
}

void hide_page::mark_vmcb_modified(svm::pvcpu_t vcpu) {
    if (!vcpu) return;
    vcpu->guest_vmcb.control_area.vmcb_clean &= ~(1ULL << 4);
}

void hide_page::flush_tlb_safe(svm::pvcpu_t vcpu) {
    if (!vcpu) return;
    KIRQL old_irql = KeRaiseIrqlToDpcLevel();
    flush_tlb_t{}.flush_tlb(vcpu, flush_tlb_t::tlb_control_id::flush_guest_tlb);
    KeLowerIrql(old_irql);
}

auto hide_page::hide_physical_page(svm::pvcpu_t vcpu, u64 page_pfn) -> svm::svmroot_error_t {
    if (!vcpu) return svm::svmroot_error_t::error_failed;

    KIRQL old_irql;
    acquire_lock(&old_irql);

    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    // Get the PTE from primary NPT
    auto primary_pte = npt::get()->get_npt_pte(vcpu->npt[npt::nested_page_table::primary], page_pfn << 12, true);
    if (!primary_pte) return svm::svmroot_error_t::error_failed;

    // Store original PFN and map to dummy page
    u64 original_pfn = primary_pte->pfn;
    primary_pte->pfn = vcpu->npt[npt::nested_page_table::primary].dummy_page_pfn;

    // Mark VMCB as modified and flush TLB
    mark_vmcb_modified(vcpu);
    flush_tlb_safe(vcpu);

    __writecr3(old_cr3);
    release_lock(old_irql);

    return svm::svmroot_error_t::error_success;
}

auto hide_page::unhide_physical_page(svm::pvcpu_t vcpu, u64 page_pfn) -> svm::svmroot_error_t {
    if (!vcpu) return svm::svmroot_error_t::error_failed;

    KIRQL old_irql;
    acquire_lock(&old_irql);

    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    bool success = false;
    do {
        // Get the PTE from primary NPT
        auto primary_pte = npt::get()->get_npt_pte(vcpu->npt[npt::nested_page_table::primary], page_pfn << 12, false);
        if (!primary_pte) break;

        // Simply map back to the original page
        primary_pte->pfn = page_pfn;

        // Mark VMCB as modified and flush TLB
        mark_vmcb_modified(vcpu);
        flush_tlb_safe(vcpu);

        success = true;
    } while (false);

    __writecr3(old_cr3);
    release_lock(old_irql);
    return success ? svm::svmroot_error_t::error_success : svm::svmroot_error_t::error_failed;
}
