#include "critical_protector.hpp"

critical_protector::critical_protector() : npt_instance(npt::get()) {
    KeInitializeSpinLock(&protector_lock);
}

void critical_protector::acquire_lock(PKIRQL old_irql) {
    KeAcquireSpinLock(&protector_lock, old_irql);
}

void critical_protector::release_lock(KIRQL old_irql) {
    KeReleaseSpinLock(&protector_lock, old_irql);
}

void critical_protector::protect_range(
    svm::pvcpu_t vcpu,
    void* virtual_address,
    size_t size,
    bool allow_write,
    bool allow_exec
) {
    if (!vcpu || !virtual_address) return;

    const u64 pages_spanned = ADDRESS_AND_SIZE_TO_SPAN_PAGES(virtual_address, size);

    for (u64 page_index = 0; page_index < pages_spanned; page_index++) {
        const u64 current_va = reinterpret_cast<u64>(virtual_address) + (page_index * PAGE_SIZE);
        const u64 physical_address = MmGetPhysicalAddress(reinterpret_cast<void*>(current_va)).QuadPart;

        // Get PTE for this physical address in both NPTs
        auto primary_pte = npt_instance->get_npt_pte(
            vcpu->npt[npt::nested_page_table::primary],
            physical_address,
            true
        );

        auto shadow_pte = npt_instance->get_npt_pte(
            vcpu->npt[npt::nested_page_table::shadow],
            physical_address,
            true
        );

        if (primary_pte) {
            // Setup primary NPT PTE
            primary_pte->value = 0;
            primary_pte->present = 1;
            primary_pte->write = allow_write ? 1 : 0;
            primary_pte->usermode = 1;
            primary_pte->no_execute = allow_exec ? 0 : 1;
            primary_pte->pfn = vcpu->npt[npt::nested_page_table::primary].dummy_page_pfn;
        }

        if (shadow_pte) {
            // Setup shadow NPT PTE with same permissions
            shadow_pte->value = 0;
            shadow_pte->present = 1;
            shadow_pte->write = allow_write ? 1 : 0;
            shadow_pte->usermode = 1;
            shadow_pte->no_execute = allow_exec ? 0 : 1;
            shadow_pte->pfn = physical_address >> 12;
        }
    }
}

void critical_protector::protect_critical_structures(svm::pvcpu_t vcpu) {
    if (!vcpu) return;

    KIRQL old_irql;
    acquire_lock(&old_irql);

    auto old_cr3 = __readcr3();

    // Get actual processor count
    ULONG processor_count = KeQueryActiveProcessorCount(nullptr);

    // Outer loop: For each VCPU object's structures that need protection
    for (u32 cpu_index = 0; cpu_index < processor_count; cpu_index++)
    {
        auto processor = hypervisor::get()->vcpus[cpu_index];
        if (!processor) continue;

        // Validate critical pointers
        if (!MmIsAddressValid(&processor->guest_vmcb) ||
            !MmIsAddressValid(&processor->host_vmcb)) {
            continue;
        }

        // Set inner processor's CR3 context
        __writecr3(processor->host_vmcb.state_save_area.cr3.value);

        // Protect the shared host page table
        protect_range(processor, &hypervisor::get()->shared_host_pt, sizeof(host_pt_t), false, false);

        // Protect critical VMCB physical addresses in host stack layout
        protect_range(processor, &processor->host_stack_layout.guest_vmcb_pa, sizeof(u64), false, false);
        protect_range(processor, &processor->host_stack_layout.host_vmcb_pa, sizeof(u64), false, false);

        // Protect shadow data structure
        protect_range(processor, &processor->host_stack_layout.shadow_data, sizeof(svm::shadow), false, false);

        // Protect shared VCPU if it exists
        if (processor->host_stack_layout.shared_vcpu &&
            MmIsAddressValid(processor->host_stack_layout.shared_vcpu)) {
            protect_range(processor, processor->host_stack_layout.shared_vcpu, sizeof(svm::shared_vcpu), false, false);
        }

        // Protect trap frame
        protect_range(processor, &processor->host_stack_layout.trap_frame, sizeof(KTRAP_FRAME), false, false);

        // Protect the outer processor's VCPU structure
        protect_range(processor, processor, sizeof(svm::vprocessor_data), false, false);

        // Protect the outer processor's VMCBs with additional validation
        protect_range(processor, &processor->guest_vmcb, sizeof(svm::vmcb_t), false, false);
        protect_range(processor, &processor->host_vmcb, sizeof(svm::vmcb_t), false, false);

        // Protect the outer processor's NPT and MSRPM
        protect_range(processor, &processor->npt[npt::nested_page_table::primary], sizeof(npt_data_t), false, false);
        protect_range(processor, &processor->npt[npt::nested_page_table::shadow], sizeof(npt_data_t), false, false);
        protect_range(processor, &processor->msrpm, sizeof(msrpm_t), false, false);

        // Protect the outer processor's state areas
        protect_range(processor, &processor->host_state_area, sizeof(processor->host_state_area), false, false);
        protect_range(processor, &processor->host_stack_layout, sizeof(processor->host_stack_layout), false, false);

        // Protect NPT paging structures
        for (u32 i = 0; i < 2; i++) {
            auto npt_type = static_cast<npt::nested_page_table>(i);

            // Protect core NPT structures
            protect_range(processor, &processor->npt[npt_type].pml4, PAGE_SIZE, false, false);
            protect_range(processor, &processor->npt[npt_type].pdpt, sizeof(processor->npt[npt_type].pdpt), false, false);
            protect_range(processor, &processor->npt[npt_type].pds, sizeof(processor->npt[npt_type].pds), false, false);
            protect_range(processor, &processor->npt[npt_type].hooks, sizeof(processor->npt[npt_type].hooks), false, false);

            // Protect dummy pages and free pages
            protect_range(processor, &processor->npt[npt_type].dummy_page, PAGE_SIZE, false, false);
            for (size_t j = 0; j < processor->npt[npt_type].num_used_free_pages; j++) {
                protect_range(processor, &processor->npt[npt_type].free_pages[j], PAGE_SIZE, false, false);
            }
        }

        // Mark VMCB as modified after protection changes
        processor->guest_vmcb.control_area.vmcb_clean &= ~(1ULL << 4); // NPT modified
        processor->guest_vmcb.control_area.vmcb_clean &= ~(1ULL << 0); // Control area modified

        // Flush TLB for this processor after all protections are applied
        flush_tlb_t{}.flush_tlb(processor, flush_tlb_t::tlb_control_id::flush_guest_tlb);
    }

    __writecr3(old_cr3);
    release_lock(old_irql);
}

critical_protector* critical_protector::get() {
    static critical_protector instance;
    return &instance;
}