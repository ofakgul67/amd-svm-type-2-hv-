#include "memory_access.hpp"

auto memory_access::get() -> memory_access* {
    static memory_access instance;
    return &instance;
}

auto memory_access::read_physical_memory(svm::pvcpu_t vcpu, u64 destination_gva, u64 source, u64 size) -> svm::svmroot_error_t {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    source += reinterpret_cast<u64>(host_physical_memory_base);
    u64 bytes_read = 0;

    while (bytes_read < size) {
        u64 destination_remaining{};
        auto destination = page_table::get()->gva_to_hva(vcpu->guest_vmcb.state_save_area.cr3,
            destination_gva + bytes_read, destination_remaining);

        if (!destination) {
            __writecr3(old_cr3);
            return svm::svmroot_error_t::svmroot_translate_failure;
        }

        u64 bytes_to_copy = min(destination_remaining, size - bytes_read);
        memcpy(reinterpret_cast<void*>(destination),
            reinterpret_cast<void*>(source + bytes_read), bytes_to_copy);

        bytes_read += bytes_to_copy;
    }

    __writecr3(old_cr3);
    return svm::svmroot_error_t::error_success;
}

auto memory_access::write_physical_memory(svm::pvcpu_t vcpu, u64 destination, u64 source_gva, u64 size) -> u64 {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    destination += reinterpret_cast<u64>(host_physical_memory_base);
    u64 bytes_written = 0;

    while (bytes_written < size) {
        u64 remaining{};
        auto source = page_table::get()->gva_to_hva(vcpu->guest_vmcb.state_save_area.cr3,
            source_gva + bytes_written, remaining);

        if (!source) {
            __writecr3(old_cr3);
            return svm::svmroot_error_t::svmroot_translate_failure;
        }

        u64 bytes_to_copy = min(remaining, size - bytes_written);
        memcpy(reinterpret_cast<void*>(destination + bytes_written),
            reinterpret_cast<void*>(source), bytes_to_copy);

        bytes_written += bytes_to_copy;
    }

    __writecr3(old_cr3);
    return svm::svmroot_error_t::error_success;
}

auto memory_access::read_virtual_memory(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 destination_gva,
    u64 source_gva, u64 size) -> svm::svmroot_error_t {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    if (!process_cr3.value)
        process_cr3.value = svm::globals::system_cr3.value;

    u64 bytes_read = 0;
    while (bytes_read < size) {
        u64 source_remaining{}, destination_remaining{};

        auto source = page_table::get()->gva_to_hva(process_cr3,
            source_gva + bytes_read, source_remaining);

        if (!source) {
            __writecr3(old_cr3);
            return svm::svmroot_error_t::svmroot_translate_failure;
        }

        auto destination = page_table::get()->gva_to_hva(vcpu->guest_vmcb.state_save_area.cr3,
            destination_gva + bytes_read, destination_remaining);

        if (!destination) {
            debug::panicf("destination invalid\n");
            __writecr3(old_cr3);
            return svm::svmroot_error_t::svmroot_translate_failure;
        }

        u64 bytes_to_copy = min(min(source_remaining, destination_remaining), size - bytes_read);
        memcpy(reinterpret_cast<void*>(destination),
            reinterpret_cast<void*>(source), bytes_to_copy);

        bytes_read += bytes_to_copy;
    }

    __writecr3(old_cr3);
    return svm::svmroot_error_t::error_success;
}

auto memory_access::write_virtual_memory(svm::pvcpu_t vcpu, svm::cr3_t process_cr3, u64 destination_gva,
    u64 source_gva, u64 size) -> svm::svmroot_error_t {
    auto old_cr3 = __readcr3();
    __writecr3(vcpu->host_vmcb.state_save_area.cr3.value);

    if (!process_cr3.value)
        process_cr3.value = svm::globals::system_cr3.value;

    u64 bytes_written = 0;
    while (bytes_written < size) {
        u64 source_remaining{}, destination_remaining{};

        auto source = page_table::get()->gva_to_hva(vcpu->guest_vmcb.state_save_area.cr3,
            source_gva + bytes_written, source_remaining);

        if (!source) {
            __writecr3(old_cr3);
            return svm::svmroot_error_t::svmroot_translate_failure;
        }

        auto destination = page_table::get()->gva_to_hva(process_cr3,
            destination_gva + bytes_written, destination_remaining);

        if (!destination) {
            __writecr3(old_cr3);
            return svm::svmroot_error_t::svmroot_translate_failure;
        }

        u64 bytes_to_copy = min(min(source_remaining, destination_remaining), size - bytes_written);
        memcpy(reinterpret_cast<void*>(destination),
            reinterpret_cast<void*>(source), bytes_to_copy);

        bytes_written += bytes_to_copy;
    }

    __writecr3(old_cr3);
    return svm::svmroot_error_t::error_success;
}
