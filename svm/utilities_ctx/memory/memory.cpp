#include "../svm/svm.hpp"
#include "memory.hpp"

#pragma warning(disable:6011)

memory::memory() noexcept {
    RTL_OSVERSIONINFOW ver = { 0 };
    RtlGetVersion(&ver);
    svm::globals::windows_build_number = ver.dwBuildNumber;
    svm::globals::offsets.kprocess_directory_table_base = 0x28;
    svm::globals::system_eprocess = reinterpret_cast<_EPROCESS*>(PsInitialSystemProcess);
    svm::globals::system_cr3 = *reinterpret_cast<svm::cr3_t*>(
        reinterpret_cast<uint8_t*>(PsInitialSystemProcess) +
        svm::globals::offsets.kprocess_directory_table_base);
}

auto memory::map_physical_memory(host_pt_t& pt) -> void {
    // map all the physical memory and share it between the hosts to be able to access physical memory directly.	
    auto& pml4e = pt.pml4[host_physical_memory_pml4_idx];
    pml4e.present = 1;
    pml4e.write = 1;
    pml4e.pfn = MmGetPhysicalAddress(&pt.pdpt).QuadPart >> 12;

    for (u64 i = 0; i < host_physical_memory_pd_count; i++) {
        auto& pdpte = pt.pdpt[i];
        pdpte.present = 1;
        pdpte.write = 1;
        pdpte.pfn = MmGetPhysicalAddress(&pt.pd[i]).QuadPart >> 12;

        for (u64 j = 0; j < 512; j++) {
            auto& pde = pt.pd[i][j];
            pde.present = 1;
            pde.write = 1;
            pde.large_page = 1;
            pde.pfn = (i << 9) + j;
        }
    }
}

auto memory::prepare_host_page_tables() -> svm::svmroot_error_t {
    auto& pt = hypervisor::get()->shared_host_pt;

    // Clear the page tables manually to guarantee alignment 
    for (size_t i = 0; i < sizeof(pt.pml4) / sizeof(pt.pml4[0]); i++) {
        pt.pml4[i].value = 0;
    }

    for (size_t i = 0; i < sizeof(pt.pdpt) / sizeof(pt.pdpt[0]); i++) {
        pt.pdpt[i].value = 0;
    }

    for (size_t i = 0; i < host_physical_memory_pd_count; i++) {
        for (size_t j = 0; j < 512; j++) {
            pt.pd[i][j].value = 0;
        }
    }

    // Map all of physical memory
    map_physical_memory(pt);

    svm::cr3_t cr3;
    cr3.value = __readcr3();
    auto guest_pml4 = reinterpret_cast<pml4e*>(get_virtual_address((void*)(cr3.pml4 << 12)));
    if (!guest_pml4) {
        return svm::svmroot_error_t::error_failed;
    }

    // Copy kernel mappings (top half)
    for (int i = 256; i < 512; i++) {
        pt.pml4[i].value = guest_pml4[i].value;
    }

    return svm::svmroot_error_t::error_success;
}
 
auto memory::alloc_contiguous_memory(u64 size) -> void* {
    PHYSICAL_ADDRESS boundary, lowest, highest;
    boundary.QuadPart = lowest.QuadPart = 0;
    highest.QuadPart = -1;

    void* memory = MmAllocateContiguousMemorySpecifyCacheNode(size,
        lowest, highest, boundary, MmCached, MM_ANY_NODE_OK);

    if (memory)
        RtlZeroMemory(memory, size);

    return memory;
}

auto memory::free_contiguous_memory(void* base) -> void {
    MmFreeContiguousMemory(base);
}

auto memory::alloc_nonpaged_pool(u64 size) -> void* {
    void* memory = ExAllocatePoolWithTag(NonPagedPool, size, 'vmm');
    if (memory) {
        NT_ASSERT(PAGE_ALIGN(memory) == memory);
        RtlZeroMemory(memory, size);
    }
    return memory;
}

auto memory::free_nonpaged_pool(void* base) -> void {
    ExFreePoolWithTag(base, 'vmm');
}

auto memory::get_virtual_address(void* physical_address) -> u64 {
    PHYSICAL_ADDRESS physical_addr{ physical_addr.QuadPart = reinterpret_cast<long long>(physical_address) };
    return reinterpret_cast<u64>(MmGetVirtualForPhysical(physical_addr));
}

auto memory::get_physical_address(void* virtual_address) -> u64 {
    return MmGetPhysicalAddress(virtual_address).QuadPart;
}

auto memory::get_eprocess(u64 target_pid) -> PEPROCESS {
    PEPROCESS process = nullptr;
    NTSTATUS status = PsLookupProcessByProcessId(reinterpret_cast<HANDLE>(target_pid), &process);
    return NT_SUCCESS(status) ? process : nullptr;
}

auto memory::get_process_cr3(u64 target_pid) -> u64 {
    if (!target_pid) {
        return 0;
    }

    for (auto current = svm::globals::system_eprocess;
        current->ActiveProcessLinks.Flink != &svm::globals::system_eprocess->ActiveProcessLinks;
        current = CONTAINING_RECORD(current->ActiveProcessLinks.Flink, _EPROCESS, ActiveProcessLinks)) {

        if (target_pid == reinterpret_cast<u64>(current->UniqueProcessId)) {
            return current->Pcb.DirectoryTableBase;
        }
    }
    return 0;
}

auto memory::get_process_peb(u64 target_pid) -> u64 {
    if (!target_pid)
        return 0;

    u64 peb_address = 0;
    PEPROCESS process = get_eprocess(target_pid);
    if (!process)
        return 0;

    PPEB peb = PsGetProcessPeb(process);
    if (peb != nullptr)
        peb_address = reinterpret_cast<u64>(peb);

    ObfDereferenceObject(process);
    return peb_address;
}

auto memory::get_process_base(u64 target_pid) -> u64 {
    if (!target_pid) {
        return svm::svmroot_error_t::invalid_pid;
    }

    for (auto current = svm::globals::system_eprocess;
        current->ActiveProcessLinks.Flink != &svm::globals::system_eprocess->ActiveProcessLinks;
        current = CONTAINING_RECORD(current->ActiveProcessLinks.Flink, _EPROCESS, ActiveProcessLinks)) {

        if (target_pid == reinterpret_cast<u64>(current->UniqueProcessId)) {
            return reinterpret_cast<u64>(current->SectionBaseAddress);
        }
    }
    return svm::svmroot_error_t::error_failed;
}

auto memory::get_current_process_cr3(u64* target_cr3) -> svm::svmroot_error_t {
    u64 process = reinterpret_cast<u64>(IoGetCurrentProcess());
    *target_cr3 = *(u64*)(process + svm::globals::offsets.kprocess_directory_table_base);
    return *target_cr3 ? svm::svmroot_error_t::error_success : svm::svmroot_error_t::error_failed;
}

auto memory::get_current_process_base(u64* target_base) -> svm::svmroot_error_t {
    *target_base = reinterpret_cast<u64>(IoGetCurrentProcess());
    return svm::svmroot_error_t::error_success;
}

auto memory::get_process_module(svm::pvcpu_t vcpu, u64 target_pid, const wchar_t* module_name) -> u64 {
    if (!vcpu || !target_pid || !module_name) {
        debug::panicf("[get_process_module] Invalid parameters\n");
        return 0;
    }

    auto process_cr3 = get_process_cr3(target_pid);
    if (!process_cr3) {
        debug::panicf("[get_process_module] Failed to get CR3 for PID: %llu\n", target_pid);
        return 0;
    }

    auto peb_address = get_process_peb(target_pid);
    if (!peb_address) {
        debug::panicf("[get_process_module] Failed to get PEB for PID: %llu\n", target_pid);
        return 0;
    }

    svm::cr3_t cr3_struct{ process_cr3 };

    PEB peb_process = { 0 };
    auto peb_physical = page_table::get()->get_physical_address(vcpu, cr3_struct, peb_address);
    if (!peb_physical) {
        debug::panicf("[get_process_module] Failed to translate PEB address: 0x%llx\n", peb_address);
        return 0;
    }

    if (memory_access::get()->read_physical_memory(vcpu, (u64)&peb_process, peb_physical, sizeof(PEB))
        != svm::svmroot_error_t::error_success) {
        debug::panicf("[get_process_module] Failed to read PEB\n");
        return 0;
    }

    if (!peb_process.Ldr) {
        debug::panicf("[get_process_module] Invalid PEB Ldr pointer\n");
        return 0;
    }

    PEB_LDR_DATA peb_ldr_data = { 0 };
    auto ldr_physical = page_table::get()->get_physical_address(vcpu, cr3_struct, (u64)peb_process.Ldr);
    if (!ldr_physical) {
        debug::panicf("[get_process_module] Failed to translate Ldr address: 0x%llx\n", (u64)peb_process.Ldr);
        return 0;
    }

    if (memory_access::get()->read_physical_memory(vcpu, (u64)&peb_ldr_data, ldr_physical, sizeof(PEB_LDR_DATA))
        != svm::svmroot_error_t::error_success) {
        debug::panicf("[get_process_module] Failed to read PEB_LDR_DATA\n");
        return 0;
    }

    if (!peb_ldr_data.ModuleListLoadOrder.Flink ||
        peb_ldr_data.ModuleListLoadOrder.Flink == peb_ldr_data.ModuleListLoadOrder.Blink) {
        debug::panicf("[get_process_module] Invalid module list pointers\n");
        return 0;
    }

    LIST_ENTRY* ldr_list_head = (LIST_ENTRY*)peb_ldr_data.ModuleListLoadOrder.Flink;
    LIST_ENTRY* ldr_current_node = ldr_list_head;
    u32 iteration_count = 0;
    const u32 MAX_ITERATIONS = 1000; // Safety limit

    do {
        if (++iteration_count > MAX_ITERATIONS) {
            debug::panicf("[get_process_module] Exceeded maximum module list iterations\n");
            return 0;
        }

        LDR_DATA_TABLE_ENTRY lst_entry = { 0 };
        auto entry_physical = page_table::get()->get_physical_address(vcpu, cr3_struct, (u64)ldr_current_node);
        if (!entry_physical) {
            debug::panicf("[get_process_module] Failed to translate entry address: 0x%llx\n", (u64)ldr_current_node);
            break;
        }

        if (memory_access::get()->read_physical_memory(vcpu, (u64)&lst_entry, entry_physical, sizeof(LDR_DATA_TABLE_ENTRY))
            != svm::svmroot_error_t::error_success) {
            debug::panicf("[get_process_module] Failed to read LDR_DATA_TABLE_ENTRY\n");
            break;
        }

        if (!lst_entry.InLoadOrderModuleList.Flink) {
            debug::panicf("[get_process_module] Invalid next module pointer\n");
            break;
        }

        ldr_current_node = lst_entry.InLoadOrderModuleList.Flink;

        if (lst_entry.BaseDllName.Length > 0 &&
            lst_entry.BaseDllName.Length < MAX_PATH * sizeof(WCHAR) &&
            lst_entry.BaseDllName.Buffer) {

            WCHAR sz_base_dll_name[MAX_PATH] = { 0 };
            auto name_physical = page_table::get()->get_physical_address(vcpu, cr3_struct, (u64)lst_entry.BaseDllName.Buffer);
            if (!name_physical) {
                continue;
            }

            if (memory_access::get()->read_physical_memory(vcpu, (u64)sz_base_dll_name, name_physical,
                min(lst_entry.BaseDllName.Length, (USHORT)(MAX_PATH * sizeof(WCHAR) - sizeof(WCHAR))))
                == svm::svmroot_error_t::error_success) {

                if (crt_strcmp(sz_base_dll_name, module_name, true)) {
                    if (lst_entry.DllBase && lst_entry.SizeOfImage) {
                        return (u64)lst_entry.DllBase;
                    }
                }
            }
        }

    } while (ldr_current_node != ldr_list_head);

    debug::panicf("[get_process_module] Module %ws not found\n", module_name);
    return 0;
}

auto memory::get() -> memory* {
    static memory instance;
    return &instance;
}
