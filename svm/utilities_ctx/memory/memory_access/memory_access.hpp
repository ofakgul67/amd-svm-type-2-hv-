#pragma once
#include "../includes.h"

class memory_access {
public:
    static auto get() -> memory_access*;

    // Physical memory operations
    auto read_physical_memory(svm::pvcpu_t vcpu, u64 destination_gva, u64 source, u64 size) -> svm::svmroot_error_t;
    auto write_physical_memory(svm::pvcpu_t vcpu, u64 destination, u64 source_gva, u64 size) -> u64;

    // Virtual memory operations 
    auto read_virtual_memory(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 destination_gva, u64 source_gva, u64 size) -> svm::svmroot_error_t;
    auto write_virtual_memory(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 destination_gva, u64 source_gva, u64 size) -> svm::svmroot_error_t;
};