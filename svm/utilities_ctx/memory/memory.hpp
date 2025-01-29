#pragma once
#include "../includes.h"

class memory {
public:
    memory() noexcept;

    // Basic memory operations
    auto alloc_contiguous_memory(u64 size) -> void*;
    auto free_contiguous_memory(void* base) -> void;
    auto alloc_nonpaged_pool(u64 size) -> void*;
    auto free_nonpaged_pool(void* base) -> void;

    // Host page table setup
    auto map_physical_memory(host_pt_t& pt) -> void;
    auto prepare_host_page_tables() -> svm::svmroot_error_t;

    // Basic address translation
    auto get_virtual_address(void* physical_address) -> u64;
    auto get_physical_address(void* virtual_address) -> u64;

    // Process operations
    auto get_eprocess(u64 target_pid) -> PEPROCESS;
    auto get_process_cr3(u64 target_pid) -> u64;
    auto get_process_module(svm::pvcpu_t vcpu, u64 target_pid, const wchar_t* module_name) -> u64;
    auto get_process_peb(u64 target_pid) -> u64;
    auto get_process_base(u64 target_pid) -> u64;

    // Current process operations
    auto get_current_process_cr3(u64* target_cr3) -> svm::svmroot_error_t;
    auto get_current_process_base(u64* target_base) -> svm::svmroot_error_t;

    static auto get() -> memory*;
};
