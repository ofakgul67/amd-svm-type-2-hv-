#include "npf_svm.hpp"

npf_handler* npf_handler::get() {
    static npf_handler instance;
    static bool is_initialized = false;

    if (!is_initialized) {
        instance.initialize();
        is_initialized = true;
    }
    return &instance;
}

void npf_handler::initialize() {
    KeInitializeSpinLock(&npf_lock);
}

void npf_handler::acquire_lock(PKIRQL old_irql) {
    KeAcquireSpinLock(&npf_lock, old_irql);
}

void npf_handler::release_lock(KIRQL old_irql) {
    KeReleaseSpinLock(&npf_lock, old_irql);
}

void npf_handler::mark_vmcb_modified(svm::pvcpu_t vcpu) {
    if (!vcpu) return;
    vcpu->guest_vmcb.control_area.vmcb_clean &= ~(1ULL << 4);  // NPT modified
}

void npf_handler::flush_tlb_if_needed(svm::pvcpu_t vcpu) {
    if (!vcpu) return;
    KIRQL old_irql = KeRaiseIrqlToDpcLevel();
    flush_tlb_t{}.flush_tlb(vcpu, flush_tlb_t::tlb_control_id::flush_guest_tlb);
    KeLowerIrql(old_irql);
}

void npf_handler::setup_pte(pte* pte, u64 pfn, bool allow_exec, bool allow_write) {
    if (!pte) return;
    pte->value = 0;
    pte->present = 1;
    pte->write = allow_write ? 1 : 0;    // Properly set write permission
    pte->usermode = 1;
    pte->no_execute = allow_exec ? 0 : 1; // Properly set execute permission
    pte->pfn = pfn;
    pte->accessed = 1;
    pte->dirty = allow_write ? 1 : 0;     // Set dirty bit when write is allowed
}

bool npf_handler::handle_not_present_fault(svm::pvcpu_t vcpu, const page_context& ctx) {
    if (!vcpu) return false;

    auto const aligned_pa = ctx.guest_pa & ~0xFFFull;
    auto const page_pfn = ctx.guest_pa >> 12;
    bool success = true;

    // Setup shadow NPT
    auto shadow_pte = npt::get()->get_npt_pte(vcpu->npt[npt::nested_page_table::shadow], aligned_pa, true);
    if (shadow_pte) {
        setup_pte(shadow_pte, page_pfn, true, true);
    }
    else {
        success = false;
    }

    // Setup primary NPT
    auto primary_pte = npt::get()->get_npt_pte(vcpu->npt[npt::nested_page_table::primary], aligned_pa, true);
    if (primary_pte) {
        setup_pte(primary_pte, page_pfn, false, true);
    }
    else {
        success = false;
    }

    if (success) {
        mark_vmcb_modified(vcpu);
        flush_tlb_if_needed(vcpu);
    }

    return success;
}

bool npf_handler::handle_execute_fault(svm::pvcpu_t vcpu, const page_context& ctx) {
    if (!vcpu) return false;

    auto const hook = npt::get()->find_npt_hook(
        vcpu->npt[npt::nested_page_table::primary],
        ctx.guest_pa >> 12);

    if (hook && hook->orig_pfn == (ctx.guest_pa >> 12)) {
        // Switch to Shadow NPT for hooked pages
        u64 shadow_npt_pa = MmGetPhysicalAddress(&vcpu->npt[npt::nested_page_table::shadow].pml4).QuadPart;
        vcpu->guest_vmcb.control_area.ncr3 = shadow_npt_pa;
        mark_vmcb_modified(vcpu);
        flush_tlb_if_needed(vcpu);
        return true;
    }

    return false;
}

bool npf_handler::handle_write_fault(svm::pvcpu_t vcpu, const page_context& ctx) {
    if (!vcpu) return false;

    auto const hook = npt::get()->find_npt_hook(
        vcpu->npt[npt::nested_page_table::primary],
        ctx.guest_pa >> 12);

    if (hook) {
        auto shadow_pte = npt::get()->get_npt_pte(
            vcpu->npt[npt::nested_page_table::shadow],
            ctx.guest_pa,
            true);

        if (shadow_pte) {
            // Enable write access in shadow NPT
            setup_pte(shadow_pte, hook->exec_pfn, true, true);

            // Important: Stay in shadow NPT for writes
            u64 shadow_npt_pa = page_table::get()->get_physical_address(
                vcpu,
                svm::cr3_t{},
                reinterpret_cast<u64>(&vcpu->npt[npt::nested_page_table::shadow].pml4));

            vcpu->guest_vmcb.control_area.ncr3 = shadow_npt_pa;
            mark_vmcb_modified(vcpu);
            flush_tlb_if_needed(vcpu);
            return true;
        }
    }

    // Only handle normal write case here (non-hooked pages)
    auto primary_pte = npt::get()->get_npt_pte(
        vcpu->npt[npt::nested_page_table::primary],
        ctx.guest_pa,
        true);

    if (!primary_pte) return false;

    setup_pte(primary_pte, ctx.guest_pa >> 12, false, true);
    mark_vmcb_modified(vcpu);
    flush_tlb_if_needed(vcpu);
    return true;
}

void npf_handler::npf_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    if (!vcpu) return;

    KIRQL old_irql;
    acquire_lock(&old_irql);

    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    auto const npt_fault_code = svm::npf_exit_info{ vcpu->guest_vmcb.control_area.exit_info1 };
    auto const guest_physical_address = vcpu->guest_vmcb.control_area.exit_info2;

    page_context ctx{
        .is_execution = npt_fault_code.fields.execute != 0,
        .is_write = npt_fault_code.fields.write != 0,
        .is_present = npt_fault_code.fields.valid != 0,
        .guest_pa = guest_physical_address,
        .host_pa = guest_physical_address
    };

    bool handled = false;

    // Check if the fault is in the TPM MMIO region
    if (tpm_handler::get()->handle_tpm_mmio(vcpu, ctx)) {
        return; // Handled by TPM handler
    }

    // Handle different fault types
    if (!ctx.is_present) {
        handled = handle_not_present_fault(vcpu, ctx);
    }
    else if (ctx.is_execution) {
        handled = handle_execute_fault(vcpu, ctx);
    }
    else if (ctx.is_write) {
        handled = handle_write_fault(vcpu, ctx);
    }

    __writecr3(old_cr3);
    release_lock(old_irql);
}