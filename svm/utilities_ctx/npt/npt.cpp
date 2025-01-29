#include "npt.hpp"

void npt::acquire_lock(PKIRQL old_irql) {
    KeAcquireSpinLock(&npt_lock, old_irql);
}

void npt::release_lock(KIRQL old_irql) {
    KeReleaseSpinLock(&npt_lock, old_irql);
}

auto npt::prepare_npt(npt_data_t& npt) -> svm::svmroot_error_t {
    memset(&npt, 0, sizeof(npt));

    npt.dummy_page_pfn = MmGetPhysicalAddress(npt.dummy_page).QuadPart >> 12;
    npt.num_used_free_pages = 0;

    // Initialize free pages
    for (size_t i = 0; i < npt_free_page_count; ++i) {
        npt.free_page_pfns[i] = MmGetPhysicalAddress(&npt.free_pages[i]).QuadPart >> 12;
    }

    // Initialize hook list
    npt.hooks.active_list_head = nullptr;
    npt.hooks.free_list_head = &npt.hooks.buffer[0];
    for (size_t i = 0; i < npt.hooks.capacity - 1; ++i) {
        npt.hooks.buffer[i].next = &npt.hooks.buffer[i + 1];
    }
    npt.hooks.buffer[npt.hooks.capacity - 1].next = nullptr;

    // Setup PML4E
    auto& pml4e = npt.pml4[0];
    pml4e.value = 0;                    // Clear all flags first
    pml4e.present = 1;                  // Page is present
    pml4e.write = 1;                    // Page is writable
    pml4e.usermode = 1;                 // User mode access allowed
    pml4e.page_write_thru = 0;          // Write-back caching
    pml4e.page_cache_disable = 0;       // Caching enabled
    pml4e.accessed = 0;                 // Not yet accessed
    pml4e.ignored6 = 0;                 // Reserved
    pml4e.reserved7 = 0;                // Must be 0
    pml4e.available_to_software = 0;    // Not used
    pml4e.no_execute = 0;               // Allow execution
    pml4e.pfn = MmGetPhysicalAddress(&npt.pdpt).QuadPart >> 12;

    // Setup PDPTEs and PDEs
    for (size_t i = 0; i < npt_pd_count; ++i) {
        // Setup PDPTE
        auto& pdpte = npt.pdpt[i];
        pdpte.value = 0;                // Clear all flags first
        pdpte.present = 1;              // Page is present
        pdpte.write = 1;                // Page is writable
        pdpte.usermode = 1;             // User mode access allowed
        pdpte.page_write_thru = 0;      // Write-back caching
        pdpte.page_cache_disable = 0;   // Caching enabled
        pdpte.accessed = 0;             // Not yet accessed
        pdpte.dirty = 0;                // Not yet written to
        pdpte.huge_page = 0;            // Not a 1GB page
        pdpte.global = 0;               // Not a global page
        pdpte.available_to_software = 0; // Not used
        pdpte.no_execute = 0;           // Allow execution
        pdpte.pfn = MmGetPhysicalAddress(&npt.pds[i]).QuadPart >> 12;

        // Setup 2MB pages
        for (size_t j = 0; j < 512; ++j) {
            auto& pde = npt.pds_2mb[i][j];
            pde.value = 0;                  // Clear all flags first
            pde.present = 1;                // Page is present
            pde.write = 1;                  // Page is writable
            pde.usermode = 1;               // User mode access allowed
            pde.page_write_thru = 0;        // Write-back caching
            pde.page_cache_disable = 0;     // Caching enabled
            pde.accessed = 0;               // Not yet accessed
            pde.dirty = 0;                  // Not yet written to
            pde.large_page = 1;             // This is a 2MB page
            pde.global = 0;                 // Not a global page
            pde.available_to_software = 0;  // Not used
            pde.pat = 0;                    // Default PAT type
            pde.reserved = 0;               // Must be 0
            pde.pfn = (i << 9) + j;        // Identity mapping
        }
    }

    return svm::svmroot_error_t::error_success;
}

auto npt::split_npt_pde(npt_data_t& npt, pde_2mb_64* const pde_2mb) -> void {
    if (!pde_2mb->large_page) return;
    if (npt.num_used_free_pages >= npt_free_page_count) return;

    auto const pt_pfn = npt.free_page_pfns[npt.num_used_free_pages];
    auto const pt = reinterpret_cast<ppte>(&npt.free_pages[npt.num_used_free_pages]);
    ++npt.num_used_free_pages;

    for (size_t i = 0; i < 512; ++i) {
        auto& pte = pt[i];
        pte.value = 0;
        pte.present = pde_2mb->present;
        pte.write = pde_2mb->write;
        pte.usermode = pde_2mb->usermode;
        pte.accessed = pde_2mb->accessed;
        pte.dirty = pde_2mb->dirty;
        pte.pfn = (pde_2mb->pfn << 9) + i;
        pte.no_execute = pde_2mb->no_execute;
    }

    auto const pde = reinterpret_cast<ppde>(pde_2mb);
    pde->value = 0;
    pde->present = 1;
    pde->write = 1;
    pde->no_execute = 0;
    pde->usermode = 1;
    pde->pfn = pt_pfn;
}

auto npt::get_npt_pte(npt_data_t& npt, u64 const physical_address, bool const force_split) -> pte* {
    virt_addr_t const addr = { physical_address };

    if (addr.pml4_index != 0) return nullptr;
    if (addr.pdpt_index >= npt_pd_count) return nullptr;

    auto& pde_2mb = npt.pds_2mb[addr.pdpt_index][addr.pd_index];
    if (pde_2mb.large_page) {
        if (!force_split) return nullptr;
        split_npt_pde(npt, &pde_2mb);
        if (pde_2mb.large_page) return nullptr;
    }

    auto const pt = reinterpret_cast<ppte>(host_physical_memory_base +
        (npt.pds[addr.pdpt_index][addr.pd_index].pfn << 12));
    return &pt[addr.pt_index];
}

void npt::setup_primary_pte(pte* pte, u64 pfn) {
    if (!pte) return;
    pte->value = 0;  // Clear all flags
    pte->present = 1;
    pte->write = 1;  // Enable write access
    pte->usermode = 1;
    pte->no_execute = 1;  // Trap executions
    pte->pfn = pfn;
    pte->accessed = 1;
    pte->dirty = 1;
}

void npt::setup_shadow_pte(pte* pte, u64 pfn) {
    if (!pte) return;
    pte->value = 0;  // Clear all flags
    pte->present = 1;
    pte->write = 1;  // Enable write access
    pte->usermode = 1;
    pte->no_execute = 0;  // Allow execution
    pte->pfn = pfn;
    pte->accessed = 1;
    pte->dirty = 1;
}

void npt::mark_vmcb_modified(svm::pvcpu_t vcpu) {
    if (!vcpu) return;
    vcpu->guest_vmcb.control_area.vmcb_clean &= ~(1ULL << 4);
}

void npt::flush_tlb_safe(svm::pvcpu_t vcpu) {
    if (!vcpu) return;
    KIRQL old_irql = KeRaiseIrqlToDpcLevel();
    flush_tlb_t{}.flush_tlb(vcpu, flush_tlb_t::tlb_control_id::flush_guest_tlb);
    KeLowerIrql(old_irql);
}

bool npt::update_hook_list(npt_data_t& npt, u64 orig_pfn, u64 exec_pfn) {
    if (!npt.hooks.free_list_head) return false;

    auto hook_node = npt.hooks.free_list_head;
    npt.hooks.free_list_head = hook_node->next;
    hook_node->next = npt.hooks.active_list_head;
    npt.hooks.active_list_head = hook_node;
    hook_node->orig_pfn = orig_pfn;
    hook_node->exec_pfn = exec_pfn;

    return true;
}

auto npt::find_npt_hook(npt_data_t& npt_data, u64 page_pfn) -> npt_hook_node_t* {
    KIRQL old_irql;
    acquire_lock(&old_irql);

    npt_hook_node_t* result = nullptr;
    for (auto curr = npt_data.hooks.active_list_head; curr; curr = curr->next) {
        if (curr->orig_pfn == page_pfn) {
            result = curr;
            break;
        }
    }

    release_lock(old_irql);
    return result;
}

auto npt::install_npt_hook(svm::pvcpu_t vcpu, npt_data_t& primary_npt,
    npt_data_t& shadow_npt, u64 original_page_pfn, u64 executable_page_pfn) -> svm::svmroot_error_t {

    KIRQL old_irql;
    acquire_lock(&old_irql);

    if (!vcpu) {
        release_lock(old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    bool success = false;
    do {
        // Set up primary NPT
        auto primary_pte = get_npt_pte(primary_npt, original_page_pfn << 12, true);
        if (!primary_pte) break;
        setup_primary_pte(primary_pte, original_page_pfn);

        // Set up shadow NPT
        auto shadow_pte = get_npt_pte(shadow_npt, original_page_pfn << 12, true);
        if (!shadow_pte) break;
        setup_shadow_pte(shadow_pte, executable_page_pfn);

        if (!update_hook_list(primary_npt, original_page_pfn, executable_page_pfn)) break;

        vcpu->guest_vmcb.control_area.ncr3 = MmGetPhysicalAddress(&primary_npt.pml4).QuadPart;
        mark_vmcb_modified(vcpu);
        flush_tlb_safe(vcpu);

        success = true;
    } while (false);

    __writecr3(old_cr3);
    release_lock(old_irql);
    return success ? svm::svmroot_error_t::error_success : svm::svmroot_error_t::error_failed;
}

auto npt::remove_npt_hook(svm::pvcpu_t vcpu, npt_data_t& npt, u64 original_page_pfn) -> svm::svmroot_error_t {
    KIRQL old_irql;
    acquire_lock(&old_irql);

    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    auto pte = get_npt_pte(npt, original_page_pfn << 12, false);
    if (!pte) {
        __writecr3(old_cr3);
        release_lock(old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    // Reset PTE to original state
    pte->present = 1;
    pte->write = 1;
    pte->no_execute = 0;
    pte->pfn = original_page_pfn;

    // Remove from hook list
    if (npt.hooks.active_list_head) {
        if (npt.hooks.active_list_head->orig_pfn == original_page_pfn) {
            auto node = npt.hooks.active_list_head;
            npt.hooks.active_list_head = node->next;
            node->next = npt.hooks.free_list_head;
            npt.hooks.free_list_head = node;
        }
        else {
            auto prev = npt.hooks.active_list_head;
            while (prev->next && prev->next->orig_pfn != original_page_pfn) {
                prev = prev->next;
            }
            if (prev->next) {
                auto node = prev->next;
                prev->next = node->next;
                node->next = npt.hooks.free_list_head;
                npt.hooks.free_list_head = node;
            }
        }
    }

    mark_vmcb_modified(vcpu);
    flush_tlb_safe(vcpu);

    __writecr3(old_cr3);
    release_lock(old_irql);
    return svm::svmroot_error_t::error_success;
}

auto npt::cleanup() -> void {
    KIRQL old_irql;
    acquire_lock(&old_irql);

    for (u32 i = 0; i < 32; i++) {
        auto vcpu = hypervisor::get()->vcpus[i];
        if (!vcpu) continue;

        auto hook_head = vcpu->npt[nested_page_table::primary].hooks.active_list_head;
        vcpu->npt[nested_page_table::primary].hooks.active_list_head = nullptr;

        while (hook_head) {
            auto current = hook_head;
            hook_head = hook_head->next;
            remove_npt_hook(vcpu, vcpu->npt[nested_page_table::primary], current->orig_pfn);
        }

        mark_vmcb_modified(vcpu);
        flush_tlb_safe(vcpu);
    }

    release_lock(old_irql);
}

auto npt::get() -> npt* {
    static npt instance;
    return &instance;
}
