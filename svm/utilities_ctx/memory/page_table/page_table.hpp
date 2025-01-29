#pragma once
#include "../includes.h"

class page_table {
public:
    page_table() = default;
    ~page_table() = default;

    // Helper functions
    auto gva_to_gpa(svm::cr3_t guest_cr3, u64 address, u64& offset_to_next_page) -> u64;
    auto gpa_to_gva(svm::cr3_t guest_cr3, u64 physical_address) -> u64;
    auto gva_to_hva(svm::cr3_t guest_cr3, u64 address, u64& offset_to_next_page) -> void*;
    auto get_physical_address(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 virtual_address) -> u64;
    auto get_virtual_address(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 physical_address) -> u64;

    // Page table management
    auto switch_to_guest_cr3(svm::pvcpu_t vcpu) -> u64;
    auto restore_cr3(u64 old_cr3) -> void;
    auto is_in_guest_context(svm::pvcpu_t vcpu) -> bool;

    static auto get() -> page_table*;
};