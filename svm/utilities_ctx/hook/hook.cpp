#include "hook.hpp"

hook::hook() : hook_count(0) {
    KeInitializeSpinLock(&hook_lock);
    RtlZeroMemory(hooks, sizeof(hooks));

    for (size_t i = 0; i < MAX_HOOKS; i++) {
        hooks[i].original_address = nullptr;
        hooks[i].hook_address = nullptr;
        hooks[i].hook_size = 0;
        hooks[i].original_pfn = 0;
        hooks[i].hook_pfn = 0;
        hooks[i].is_active = false;
        hooks[i].original_mdl = nullptr;
        hooks[i].hook_mdl = nullptr;
    }
}

auto hook::lock_pages(void* address, size_t size, PMDL* out_mdl) -> bool {
    if (!address || !size || !out_mdl ||
        size > MAXULONG ||
        reinterpret_cast<ULONG_PTR>(address) + size < reinterpret_cast<ULONG_PTR>(address)) {
        return false;
    }

    PMDL mdl = nullptr;
    __try {
        mdl = IoAllocateMdl(address, static_cast<ULONG>(size), FALSE, FALSE, nullptr);
        if (!mdl) return false;

        MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
        *out_mdl = mdl;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (mdl) IoFreeMdl(mdl);
        return false;
    }
}

auto hook::unlock_pages(PMDL mdl) -> void {
    if (!mdl) return;

    __try {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silent cleanup failure - already in exception handler
    }
}

auto hook::install_function_hook(svm::pvcpu_t vcpu, void* target, void* hook_func, void** original) -> svm::svmroot_error_t {
    if (!target || !hook_func || !original ||
        !MmIsAddressValid(target) || !MmIsAddressValid(hook_func)) {
        return svm::svmroot_error_t::error_failed;
    }

    KIRQL old_irql;
    KeAcquireSpinLock(&hook_lock, &old_irql);

    if (hook_count >= MAX_HOOKS) {
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    auto hook_size = utilities::get_patch_size(target, 14);
    if (hook_size < 14) {
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    // Allocate executable page for the hook
    auto executable_page = memory::get()->alloc_nonpaged_pool(PAGE_SIZE);
    if (!executable_page) {
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    // Create the hook code in the executable page
    u8 hook_bytes[] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // jmp qword ptr [rip+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // address
    };
    *reinterpret_cast<u64*>(&hook_bytes[6]) = reinterpret_cast<u64>(hook_func);
    RtlCopyMemory(executable_page, hook_bytes, sizeof(hook_bytes));

    // Create trampoline
    auto trampoline = memory::get()->alloc_nonpaged_pool(hook_size + 14);
    if (!trampoline) {
        memory::get()->free_nonpaged_pool(executable_page);
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    // Copy original bytes to trampoline
    RtlCopyMemory(trampoline, target, hook_size);

    // Add jump back to original function after trampoline
    auto jump_back = reinterpret_cast<u8*>(trampoline) + hook_size;
    u8 jump_back_bytes[] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // jmp qword ptr [rip+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // address
    };
    *reinterpret_cast<u64*>(&jump_back_bytes[6]) = reinterpret_cast<u64>(target) + hook_size;
    RtlCopyMemory(jump_back, jump_back_bytes, sizeof(jump_back_bytes));

    // Get physical page numbers
    auto target_pa = page_table::get()->get_physical_address(vcpu, svm::cr3_t{}, reinterpret_cast<u64>(target));
    auto hook_pa = page_table::get()->get_physical_address(vcpu, svm::cr3_t{}, reinterpret_cast<u64>(executable_page));

    if (!target_pa || !hook_pa) {
        memory::get()->free_nonpaged_pool(executable_page);
        memory::get()->free_nonpaged_pool(trampoline);
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    auto target_pfn = target_pa >> 12;
    auto hook_pfn = hook_pa >> 12;

    // Install NPT hooks on all processors
    bool success = true;
    auto processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    for (u32 i = 0; i < processor_count; i++) {
        auto current_vcpu = hypervisor::get()->vcpus[i];
        if (!current_vcpu) continue;

        auto result = npt::get()->install_npt_hook(
            current_vcpu,
            current_vcpu->npt[npt::nested_page_table::primary],
            current_vcpu->npt[npt::nested_page_table::shadow],
            target_pfn,
            hook_pfn
        );

        if (result != svm::svmroot_error_t::error_success) {
            success = false;
            break;
        }
    }

    if (!success) {
        // Cleanup on failure
        memory::get()->free_nonpaged_pool(executable_page);
        memory::get()->free_nonpaged_pool(trampoline);
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    // Store hook information
    hooks[hook_count].original_address = target;
    hooks[hook_count].hook_address = hook_func;
    hooks[hook_count].hook_size = hook_size;
    hooks[hook_count].original_pfn = target_pfn;
    hooks[hook_count].hook_pfn = hook_pfn;
    hooks[hook_count].is_active = true;
    _InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&hook_count));

    *original = trampoline;
    KeReleaseSpinLock(&hook_lock, old_irql);
    return svm::svmroot_error_t::error_success;
}

auto hook::remove_function_hook(svm::pvcpu_t vcpu, void* target) -> svm::svmroot_error_t {
    if (!target || !MmIsAddressValid(target)) {
        return svm::svmroot_error_t::error_failed;
    }

    KIRQL old_irql;
    KeAcquireSpinLock(&hook_lock, &old_irql);

    // Find the hook entry
    hook_context* target_hook = nullptr;
    for (size_t i = 0; i < hook_count; i++) {
        if (hooks[i].original_address == target && hooks[i].is_active) {
            target_hook = &hooks[i];
            break;
        }
    }

    if (!target_hook) {
        KeReleaseSpinLock(&hook_lock, old_irql);
        return svm::svmroot_error_t::error_failed;
    }

    // Remove NPT hooks from all processors
    bool success = true;
    auto processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    for (u32 i = 0; i < processor_count; i++) {
        auto current_vcpu = hypervisor::get()->vcpus[i];
        if (!current_vcpu) continue;

        auto result = npt::get()->remove_npt_hook(
            current_vcpu,
            current_vcpu->npt[npt::nested_page_table::primary],
            target_hook->original_pfn
        );

        if (result != svm::svmroot_error_t::error_success) {
            success = false;
            break;
        }
    }

    if (success) {
        target_hook->is_active = false;
        _InterlockedDecrement64(reinterpret_cast<volatile LONG64*>(&hook_count));
    }

    KeReleaseSpinLock(&hook_lock, old_irql);
    return success ? svm::svmroot_error_t::error_success : svm::svmroot_error_t::error_failed;
}

auto hook::get() -> hook* {
    static hook instance;
    return &instance;
}