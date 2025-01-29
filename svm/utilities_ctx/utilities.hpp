#pragma once
#include "../includes.h"

class utilities {
public:
    // CPU and Virtualization Support
    static auto is_amd() -> bool;
    static auto is_svm_supported() -> bool;
    static auto is_npt_supported() -> bool;
    static auto is_svm_disabled_at_boot() -> bool;
    static auto is_hypervisor_vendor_installed() -> bool;
    static auto check_support() -> bool;
    static auto can_svm_be_enabled() -> bool;
    static auto enable_svme() -> void;

    // Memory Management
    static auto lock_pages(void* virtual_address, LOCK_OPERATION operation, int size = 4096) -> PMDL;
    static auto unlock_pages(PMDL mdl) -> NTSTATUS;
    static auto copy_memory_safe(void* dest, void* src, unsigned long long length) -> bool;
    static auto page_frame_number_to_virtual_address(u64 page_frame_number) -> void*;

    // Process and System Information
    static auto execute_on_each_processor(callback callback, PVOID ctx) -> bool;
    static auto get_process_id_by_name(const wchar_t* process_name) -> void*;
    static auto get_windows_version() -> void;

    // Module and Function Handling
    static auto get_section(void* image_base, const char* section_name) -> PIMAGE_SECTION_HEADER;
    static auto get_exported_function(const unsigned long long module_address, const char* proc_name) -> unsigned long long;

    // Miscellaneous Utilities
    static auto get_patch_size(PVOID code, ULONG length_hook) -> ULONG;
    static auto exponent(int base, int power) -> int;
};
