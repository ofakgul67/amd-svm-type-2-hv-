#include "rdtsc_svm.hpp"
#include <intrin.h>

u8 rdtsc_handler::get_current_cpu() {
    return static_cast<u8>(KeGetCurrentProcessorNumber());
}

rdtsc_handler* rdtsc_handler::get() {
    static rdtsc_handler instance;
    return &instance;
}

void rdtsc_handler::rdtsc_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u8 cpu_index = get_current_cpu();
    if (cpu_index >= MAX_CPUS) return;

    auto& data = cpu_data[cpu_index];
    u64 real_tsc = __rdtsc();

    if (data.rdtsc_prev == 0) {
        data.rdtsc_prev = real_tsc;
        data.rdtsc_fake = real_tsc;
    }

    if (real_tsc > data.rdtsc_prev) {
        // Use lower 4 bits (& 0xF) to get a value between 0-15
        // Add 50 to get range of 50-65 cycles
        u64 random_cycles = 50 + (real_tsc & 0xF);
        data.rdtsc_fake += random_cycles;
    }

    if (data.rdtsc_fake > real_tsc) {
        data.rdtsc_fake = real_tsc;
    }

    data.rdtsc_prev = real_tsc;
    guest_ctx->vprocessor_registers->rax.value = data.rdtsc_fake & MAXUINT;
    guest_ctx->vprocessor_registers->rdx.value = (data.rdtsc_fake >> 32) & MAXUINT;
    vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
}