#include "cr_svm.hpp"

cr_handler* cr_handler::get() {
    static cr_handler instance;
    static bool is_initialized = false;

    if (!is_initialized) {
        instance.initialize();
        is_initialized = true;
    }
    return &instance;
}

void cr_handler::initialize() {
    RtlZeroMemory(&cpu_state, sizeof(cpu_state));

    config.enforce_smep = true;
    config.enforce_smap = true;
    config.track_cr3 = true;
    config.shadow_cr4 = true;
}

u8 cr_handler::get_current_cpu() {
    return static_cast<u8>(KeGetCurrentProcessorNumber());
}

void cr_handler::init_shadow_state(u8 cpu_index) {
    if (!cpu_state[cpu_index].initialized) {
        cpu_state[cpu_index].shadow_cr3 = __readcr3();
        cpu_state[cpu_index].shadow_cr4 = __readcr4();
        cpu_state[cpu_index].cr3_switches = 0;
        cpu_state[cpu_index].last_cr3_switch_time = __rdtsc();
        cpu_state[cpu_index].initialized = true;
    }
}

u64* cr_handler::get_register_for_cr_exit(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    const u64 gpr_num = vcpu->guest_vmcb.control_area.exit_info1 & 0xF;

    switch (gpr_num) {
    case 0: return &guest_ctx->vprocessor_registers->rax.value;
    case 1: return &guest_ctx->vprocessor_registers->rcx.value;
    case 2: return &guest_ctx->vprocessor_registers->rdx.value;
    case 3: return &guest_ctx->vprocessor_registers->rbx.value;
    case 4: return &guest_ctx->vprocessor_registers->rsp.value;
    case 5: return &guest_ctx->vprocessor_registers->rbp.value;
    case 6: return &guest_ctx->vprocessor_registers->rsi.value;
    case 7: return &guest_ctx->vprocessor_registers->rdi.value;
    case 8: return &guest_ctx->vprocessor_registers->r8.value;
    case 9: return &guest_ctx->vprocessor_registers->r9.value;
    case 10: return &guest_ctx->vprocessor_registers->r10.value;
    case 11: return &guest_ctx->vprocessor_registers->r11.value;
    case 12: return &guest_ctx->vprocessor_registers->r12.value;
    case 13: return &guest_ctx->vprocessor_registers->r13.value;
    case 14: return &guest_ctx->vprocessor_registers->r14.value;
    case 15: return &guest_ctx->vprocessor_registers->r15.value;
    default: return nullptr;
    }
}

bool cr_handler::handle_cr4_write(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u8 cpu_index = get_current_cpu();
    init_shadow_state(cpu_index);

    u64* reg = get_register_for_cr_exit(vcpu, guest_ctx);
    if (!reg) {
        event_inj_t{}.general_protection_exception(vcpu);
        return false;
    }

    svm::cr4_t new_cr4{};
    new_cr4.value = *reg;

    // Check for PCIDE activation with invalid CR3
    if (new_cr4.pcide == 1) {
        svm::cr3_t cr3;
        cr3 = vcpu->guest_vmcb.state_save_area.cr3;
        if (cr3.value & 0xFFF) {
            event_inj_t{}.general_protection_exception(vcpu);
            return false;
        }
    }

    // Handle reserved bits
    if (new_cr4.reserved13 || new_cr4.reserved19 || new_cr4.reserved24) {
        event_inj_t{}.general_protection_exception(vcpu);
        return false;
    }

    // Apply security policies if configured
    if (config.enforce_smep) {
        new_cr4.smep = 1;
    }
    if (config.enforce_smap) {
        new_cr4.smap = 1;
    }

    // Update shadow and real CR4
    cpu_state[cpu_index].shadow_cr4 = new_cr4.value;
    vcpu->guest_vmcb.state_save_area.cr4 = new_cr4;

    // Flush TLB on CR4 change
    vcpu->guest_vmcb.control_area.tlb_control = 1;
    return true;
}

bool cr_handler::handle_cr4_read(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u8 cpu_index = get_current_cpu();
    init_shadow_state(cpu_index);

    u64* reg = get_register_for_cr_exit(vcpu, guest_ctx);
    if (!reg) {
        event_inj_t{}.general_protection_exception(vcpu);
        return false;
    }

    *reg = cpu_state[cpu_index].shadow_cr4;
    return true;
}

bool cr_handler::handle_cr3_write(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u8 cpu_index = get_current_cpu();
    init_shadow_state(cpu_index);

    u64* reg = get_register_for_cr_exit(vcpu, guest_ctx);
    if (!reg) {
        event_inj_t{}.general_protection_exception(vcpu);
        return false;
    }

    // Track CR3 changes if configured
    if (config.track_cr3) {
        cpu_state[cpu_index].cr3_switches++;
        cpu_state[cpu_index].last_cr3_switch_time = __rdtsc();
        cpu_state[cpu_index].shadow_cr3 = *reg;
    }

    // Update CR3 and flush TLB
    vcpu->guest_vmcb.state_save_area.cr3.value = *reg;
    vcpu->guest_vmcb.control_area.tlb_control = 7; // Clear guest non-global TLB
    return true;
}

bool cr_handler::handle_cr3_read(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u64* reg = get_register_for_cr_exit(vcpu, guest_ctx);
    if (!reg) {
        event_inj_t{}.general_protection_exception(vcpu);
        return false;
    }

    *reg = vcpu->guest_vmcb.state_save_area.cr3.value;
    return true;
}

bool cr_handler::cr_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    bool status = false;

    // Get exit code for specific SVM instruction
    auto exit_code = vcpu->guest_vmcb.control_area.exit_code;

    switch (exit_code) {
    case vmexit::cr3_read:
        status = handle_cr3_read(vcpu, guest_ctx);
        break;

    case vmexit::cr3_write:
        status = handle_cr3_write(vcpu, guest_ctx);
        break;

    case vmexit::cr4_read:
        status = handle_cr4_read(vcpu, guest_ctx);
        break;

    case vmexit::cr4_write:
        status = handle_cr4_write(vcpu, guest_ctx);
        break;

    default:
        event_inj_t{}.general_protection_exception(vcpu);
        return false;
    }

    if (status) {
        vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
    }

    return status;
}
