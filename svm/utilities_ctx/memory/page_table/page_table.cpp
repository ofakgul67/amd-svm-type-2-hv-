#include "page_table.hpp"

auto page_table::gva_to_gpa(svm::cr3_t guest_cr3, u64 address, u64& offset_to_next_page) -> u64 {
    static constexpr uint64_t pdpt_address_range = 0x40000000; // 1GB
    static constexpr uint64_t pd_address_range = 0x200000;     // 2MB
    static constexpr uint64_t pt_address_range = 0x1000;       // 4KB

    uint64_t offset{};
    _virt_addr_t const va{ address };
    auto& base = host_physical_memory_base;

    // Get PML4 entry
    auto const pml4 = reinterpret_cast<ppml4e>(
        base + (guest_cr3.pml4 << 12));
    auto const pml4e = pml4[va.pml4_index];

    if (!pml4e.present) {
        debug::panicf("PML4 entry not present for address: 0x%lx, index: %u\n", address, va.pml4_index);
        return {};
    }

    // Get PDPT entry
    auto const pdpt = reinterpret_cast<ppdpte>(base + (pml4e.pfn << 12));
    auto const pdpte = pdpt[va.pdpt_index];

    if (!pdpte.present) {
        debug::panicf("PDPT entry not present for address: 0x%p, index: %u\n", address, va.pdpt_index);
        return {};
    }

    // Handle 1GB page
    if (pdpte.huge_page) {
        offset = (va.pd_index << 21) + (va.pt_index << 12) + va.offset_4kb;
        offset_to_next_page = pdpt_address_range - offset;
        // Preserve all address bits for 1GB page
        return ((pdpte.pfn << 12) & ~((1ULL << 30) - 1)) | (offset & ((1ULL << 30) - 1));
    }

    // Get PD entry
    auto const pd = reinterpret_cast<ppde>(base + (pdpte.pfn << 12));
    auto const pde = pd[va.pd_index];

    if (!pde.present) {
        debug::panicf("PD entry not present for address: 0x%lx, index: %u\n", address, va.pd_index);
        return {};
    }

    // Handle 2MB page
    if (pde.large_page) {
        offset = (va.pt_index << 12) + va.offset_4kb;
        offset_to_next_page = pd_address_range - offset;
        // Preserve all address bits for 2MB page
        return ((pde.pfn << 12) & ~((1ULL << 21) - 1)) | (offset & ((1ULL << 21) - 1));
    }

    // Get PT entry
    auto const pt = reinterpret_cast<ppte>(base + (pde.pfn << 12));
    auto const pte = pt[va.pt_index];

    if (!pte.present) {
        debug::panicf("PT entry not present for address: 0x%lx, index: %u\n", address, va.pt_index);
        return {};
    }

    // Handle 4KB page
    offset = va.offset_4kb;
    offset_to_next_page = pt_address_range - offset;
    // Preserve all address bits for 4KB page
    return (pte.pfn << 12) | offset;
}

auto page_table::gpa_to_gva(svm::cr3_t guest_cr3, u64 physical_address) -> u64 {
    auto pml4 = reinterpret_cast<ppml4e>(host_physical_memory_base + (guest_cr3.pml4 << 12));
    physical_address &= ~0xFFF; // Clear offset bits

    // Scan through all possible virtual addresses
    for (u64 pml4_index = 0; pml4_index < 512; pml4_index++) {
        auto& pml4e = pml4[pml4_index];
        if (!pml4e.present)
            continue;

        auto pdpt = reinterpret_cast<ppdpte>(host_physical_memory_base + (pml4e.pfn << 12));
        for (u64 pdpt_index = 0; pdpt_index < 512; pdpt_index++) {
            auto& pdpte = pdpt[pdpt_index];
            if (!pdpte.present)
                continue;

            if (pdpte.huge_page) {
                u64 page_base = (pdpte.pfn << 12) & ~((1ULL << 30) - 1);
                if (physical_address >= page_base && physical_address < page_base + 0x40000000) {
                    u64 offset = physical_address - page_base;
                    return (pml4_index << 39) | (pdpt_index << 30) | offset;
                }
                continue;
            }

            auto pd = reinterpret_cast<ppde>(host_physical_memory_base + (pdpte.pfn << 12));
            for (u64 pd_index = 0; pd_index < 512; pd_index++) {
                auto& pde = pd[pd_index];
                if (!pde.present)
                    continue;

                if (pde.large_page) {
                    u64 page_base = (pde.pfn << 12) & ~((1ULL << 21) - 1);
                    if (physical_address >= page_base && physical_address < page_base + 0x200000) {
                        u64 offset = physical_address - page_base;
                        return (pml4_index << 39) | (pdpt_index << 30) | (pd_index << 21) | offset;
                    }
                    continue;
                }

                auto pt = reinterpret_cast<ppte>(host_physical_memory_base + (pde.pfn << 12));
                for (u64 pt_index = 0; pt_index < 512; pt_index++) {
                    auto& pte = pt[pt_index];
                    if (!pte.present)
                        continue;

                    u64 page_base = pte.pfn << 12;
                    if (physical_address == page_base) {
                        return (pml4_index << 39) | (pdpt_index << 30) | (pd_index << 21) | (pt_index << 12);
                    }
                }
            }
        }
    }

    return 0; // Not found
}

auto page_table::gva_to_hva(svm::cr3_t guest_cr3, u64 address, u64& offset_to_next_page) -> void* {
    uint64_t gpa = gva_to_gpa(guest_cr3, address, offset_to_next_page);
    if (!gpa)
        return nullptr;

    return gpa + host_physical_memory_base;
}

auto page_table::get_physical_address(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 virtual_address) -> u64 {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    if (!process_cr3.value)
        process_cr3.value = svm::globals::system_cr3.value;

    u64 offset_to_next_page{};
    u64 physical_address = gva_to_gpa(process_cr3, virtual_address, offset_to_next_page);
    if (!physical_address) {
        __writecr3(old_cr3);
        return svm::svmroot_error_t::svmroot_translate_failure;
    }

    physical_address &= 0xFFFFFFFFFULL;

    __writecr3(old_cr3);
    return physical_address;
}

auto page_table::get_virtual_address(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 physical_address) -> u64 {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    if (!process_cr3.value)
        process_cr3.value = svm::globals::system_cr3.value;

    u64 virtual_address = gpa_to_gva(process_cr3, physical_address);
    if (!virtual_address) {
        __writecr3(old_cr3);
        return svm::svmroot_error_t::svmroot_translate_failure;
    }

    virtual_address |= 0xFFFF000000000000;

    __writecr3(old_cr3);
    return virtual_address;
}

auto page_table::switch_to_guest_cr3(svm::pvcpu_t vcpu) -> u64 {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->guest_vmcb.state_save_area.cr3.value);
    return old_cr3;
}

auto page_table::restore_cr3(u64 old_cr3) -> void {
    __writecr3(old_cr3);
}

auto page_table::is_in_guest_context(svm::pvcpu_t vcpu) -> bool {
    u64 current_cr3 = __readcr3();
    u64 guest_cr3 = vcpu->guest_vmcb.state_save_area.cr3.value;
    return current_cr3 == guest_cr3;
}

auto page_table::get() -> page_table* {
    static page_table instance;
    return &instance;
}