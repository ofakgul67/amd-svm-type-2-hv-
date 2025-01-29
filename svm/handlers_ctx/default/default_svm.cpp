#include "default_svm.hpp"

auto svm_handler::get() -> svm_handler* {
    static svm_handler instance;
    static bool is_initialized = false;

    if (!is_initialized) {
        instance.initialize();
        is_initialized = true;
    }
    return &instance;
}

void svm_handler::initialize() {
    // Set default configuration
    config.enforce_svm_lock = true;
    config.prevent_nested_virtualization = true;
    config.log_svm_exits = false;
}

bool svm_handler::check_svm_enabled(svm::pvcpu_t vcpu) {
    return vcpu->host_stack_layout.shadow_data.efer.svme != 0;
}

void svm_handler::inject_invalid_opcode(svm::pvcpu_t vcpu) {
    event_inj_t{}.invalid_opcode_exception(vcpu);
}

void svm_handler::advance_rip(svm::pvcpu_t vcpu) {
    vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
}

void svm_handler::handle_mwait(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    inject_invalid_opcode(vcpu);
}

void svm_handler::handle_mwait_conditional(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    inject_invalid_opcode(vcpu);
}

void svm_handler::handle_monitor(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    inject_invalid_opcode(vcpu);
}

void svm_handler::handle_vmrun(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    if (config.prevent_nested_virtualization) {
        inject_invalid_opcode(vcpu);
        return;
    }

    if (!check_svm_enabled(vcpu)) {
        inject_invalid_opcode(vcpu);
        return;
    }

    advance_rip(vcpu);
}

void svm_handler::handle_vmload_vmsave(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    if (!check_svm_enabled(vcpu)) {
        inject_invalid_opcode(vcpu);
        return;
    }

    // Block VMLOAD/VMSAVE when nested virtualization is prevented
    if (config.prevent_nested_virtualization) {
        inject_invalid_opcode(vcpu);
        return;
    }

    advance_rip(vcpu);
}

void svm_handler::handle_clgi_stgi(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    if (!check_svm_enabled(vcpu)) {
        inject_invalid_opcode(vcpu);
        return;
    }

    advance_rip(vcpu);
}

void svm_handler::handle_skinit(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    // Always block SKINIT for security
    inject_invalid_opcode(vcpu);
}

void svm_handler::handle_rdtscp(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    inject_invalid_opcode(vcpu);
}

void svm_handler::handle_icebp(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    inject_invalid_opcode(vcpu);
}

void svm_handler::handle_invlpga(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    inject_invalid_opcode(vcpu);
}

void svm_handler::default_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    if (config.log_svm_exits) {
        debug::panicf("[SVM] Handling SVM instruction exit: %d\n",
            vcpu->guest_vmcb.control_area.exit_code);
    }

    // Get exit code for specific SVM instruction
    auto exit_code = vcpu->guest_vmcb.control_area.exit_code;

    switch (exit_code) {
    case vmexit::mwait:
        handle_mwait(vcpu, guest_ctx);
        break;

    case vmexit::mwait_conditional:
        handle_mwait_conditional(vcpu, guest_ctx);
        break;

    case vmexit::monitor:
        handle_monitor(vcpu, guest_ctx);
        break;

    case vmexit::vmrun:
        handle_vmrun(vcpu, guest_ctx);
        break;

    case vmexit::vmload:
    case vmexit::vmsave:
        handle_vmload_vmsave(vcpu, guest_ctx);
        break;

    case vmexit::clgi:
    case vmexit::sigi:
        handle_clgi_stgi(vcpu, guest_ctx);
        break;

    case vmexit::skinit:
        handle_skinit(vcpu, guest_ctx);
        break;

    case vmexit::rdtscp:
        handle_rdtscp(vcpu, guest_ctx);
        break;

    case vmexit::icebp:
        handle_icebp(vcpu, guest_ctx);
        break;

    case vmexit::invlpga:
        handle_invlpga(vcpu, guest_ctx);
        break;

    default:
        // Unexpected SVM exit - inject #UD
        inject_invalid_opcode(vcpu);
        break;
    }
}
