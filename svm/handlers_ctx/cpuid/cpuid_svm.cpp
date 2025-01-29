#include "cpuid_svm.hpp"

cpuid_handler* cpuid_handler::get() {
    static cpuid_handler instance;
    static bool is_initialized = false;

    if (!is_initialized) {
        instance.initialize();
        is_initialized = true;
    }
    return &instance;
}

void cpuid_handler::initialize() {
    config.stealth_mode = true;
    config.hide_features = true;
}

u8 cpuid_handler::get_current_cpu() {
    return static_cast<u8>(KeGetCurrentProcessorNumber());
}

cpuid_registers_t cpuid_handler::get_raw_cpuid(u32 leaf, u32 subleaf) {
    cpuid_registers_t regs;
    int cpuid_data[4];
    __cpuidex(cpuid_data, leaf, subleaf);
    regs.eax = cpuid_data[0];
    regs.ebx = cpuid_data[1];
    regs.ecx = cpuid_data[2];
    regs.edx = cpuid_data[3];
    return regs;
}

void cpuid_handler::handle_basic_leaf(u32 leaf, u32 subleaf, cpuid_registers_t& regs) {
    regs = get_raw_cpuid(leaf, subleaf);

    switch (leaf) {
    case 1:
        if (config.hide_features) {
            regs.ecx &= ~(1U << 31); // Clear hypervisor bit
        }
        break;
    }
}

void cpuid_handler::handle_extended_leaf(u32 leaf, u32 subleaf, cpuid_registers_t& regs) {
    regs = get_raw_cpuid(leaf, subleaf);

    switch (leaf) {
    case 0x80000001:
        if (config.hide_features) {
            //regs.ecx &= ~(1U << 2); // Clear SVM bit only
        }
        break;

    case 0x8000000A:
        if (config.stealth_mode) {
            RtlZeroMemory(&regs, sizeof(regs));
            regs.edx &= ~(1U << 1);  // Clear LbrVirt (Last Branch Records virtualization) bit
        }
        break;
    }
}

void cpuid_handler::cpuid_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u32 leaf = guest_ctx->vprocessor_registers->rax.value & MAXUINT;
    u32 subleaf = guest_ctx->vprocessor_registers->rcx.value & MAXUINT;
    cpuid_registers_t regs;

    switch (leaf) {
    case 0x40000000: // Vendor ID
    case 0x40000001: // Interface Version
    case 0x40000002: // System Identity
    case 0x40000003: // Feature Identification
    case 0x40000004: // Implementation Recommendations
    case 0x40000005: // Implementation Limits
    case 0x40000006: // Hardware Features
        if (config.stealth_mode) {
            RtlZeroMemory(&regs, sizeof(regs));
        }
        else {
            regs = get_raw_cpuid(leaf, subleaf);
        }
        break;

    case 0x80000001:
    case 0x8000000A:
        handle_extended_leaf(leaf, subleaf, regs);
        break;

    default:
        if (leaf < 0x80000000) {
            handle_basic_leaf(leaf, subleaf, regs);
        }
        else {
            regs = get_raw_cpuid(leaf, subleaf);
        }
        break;
    }

    guest_ctx->vprocessor_registers->rax.value = regs.eax;
    guest_ctx->vprocessor_registers->rbx.value = regs.ebx;
    guest_ctx->vprocessor_registers->rcx.value = regs.ecx;
    guest_ctx->vprocessor_registers->rdx.value = regs.edx;

    vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
}