#include "xsetbv_svm.hpp"

xsetbv_handler* xsetbv_handler::get() {
    static xsetbv_handler instance;
    return &instance;
}

bool xsetbv_handler::is_valid_xcr0(u64 value) {
    // x87 FPU state (bit 0) must be set
    if (!(value & 1))
        return false;

    // AVX state (bit 2) requires SSE state (bit 1)
    if ((value & (1ULL << 2)) && !(value & (1ULL << 1)))
        return false;

    // Both BNDREGS and BNDCSR must be set or cleared together
    bool bndregs = (value & (1ULL << 3)) != 0;
    bool bndcsr = (value & (1ULL << 4)) != 0;
    if (bndregs != bndcsr)
        return false;

    // AVX-512 requires YMM
    if (value & ((1ULL << 5) | (1ULL << 6) | (1ULL << 7))) {
        if (!(value & (1ULL << 2)))
            return false;
    }

    return true;
}

void xsetbv_handler::xsetbv_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u32 xcr = guest_ctx->vprocessor_registers->rcx.value & MAXUINT;
    u64 value = (static_cast<u64>(guest_ctx->vprocessor_registers->rdx.value) << 32) |
        guest_ctx->vprocessor_registers->rax.value;

    if (xcr != 0) {
        event_inj_t{}.general_protection_exception(vcpu);
        return;
    }

    if (!is_valid_xcr0(value)) {
        event_inj_t{}.general_protection_exception(vcpu);
        return;
    }

    _xsetbv(xcr, value);
    vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
}
