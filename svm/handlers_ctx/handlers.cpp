#include "handlers.hpp"

extern "C" bool handle_vmexit(svm::pvcpu_t vcpu, svm::pguest_registers_t guest_registers) {
    svm::guest_ctx_t guest_ctx{};

    // load some of the host state which isn't loaded on vmexit
    __svm_vmload(vcpu->host_stack_layout.host_vmcb_pa);

    NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);

    // the guest's rax is overwritten by the hosts on vmexit 
    // and saved in the vmcb instead
    guest_registers->rax = vcpu->guest_vmcb.state_save_area.rax;

    guest_ctx.vprocessor_registers = guest_registers;
    guest_ctx.should_exit = false;

    // update the _KTRAP_FRAME struct values in hypervisor stack, 
    // so that windbg can reconstruct the guests stack frame
    vcpu->host_stack_layout.trap_frame.Rsp = vcpu->guest_vmcb.state_save_area.rsp;
    vcpu->host_stack_layout.trap_frame.Rip = vcpu->guest_vmcb.control_area.next_rip;

    switch (vcpu->guest_vmcb.control_area.exit_code) {
        case vmexit::cpuid: {
            cpuid_handler::get()->cpuid_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::msr: {
            msr_handler::get()->msr_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::vmcall: {
            vmmcall_handler::get()->vmmcall_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::npf: {
            npf_handler::get()->npf_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::excp6_write: {
            ud_handler::get()->ud_exception_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::rdtsc: {
            rdtsc_handler::get()->rdtsc_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::dr0_read:
        case vmexit::dr1_read:
        case vmexit::dr2_read:
        case vmexit::dr3_read:
        case vmexit::dr4_read:
        case vmexit::dr5_read:
        case vmexit::dr6_read:
        case vmexit::dr7_read:
        case vmexit::dr8_read:
        case vmexit::dr9_read:
        case vmexit::dr10_read:
        case vmexit::dr11_read:
        case vmexit::dr12_read:
        case vmexit::dr13_read:
        case vmexit::dr14_read:
        case vmexit::dr15_read:
        case vmexit::dr0_write:
        case vmexit::dr1_write:
        case vmexit::dr2_write:
        case vmexit::dr3_write:
        case vmexit::dr4_write:
        case vmexit::dr5_write:
        case vmexit::dr6_write:
        case vmexit::dr7_write:
        case vmexit::dr8_write:
        case vmexit::dr9_write:
        case vmexit::dr10_write:
        case vmexit::dr11_write:
        case vmexit::dr12_write:
        case vmexit::dr13_write:
        case vmexit::dr14_write:
        case vmexit::dr15_write: {
            debug_handler::get()->debug_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::cr4_read:
        case vmexit::cr4_write:
        case vmexit::cr3_read:
        case vmexit::cr3_write: {
            cr_handler::get()->cr_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::mwait:
        case vmexit::mwait_conditional:
        case vmexit::monitor:
        case vmexit::vmrun:
        case vmexit::vmload:
        case vmexit::vmsave:
        case vmexit::sigi:
        case vmexit::clgi:
        case vmexit::skinit:
        case vmexit::rdtscp:
        case vmexit::icebp:
        case vmexit::invlpga:
            svm_handler::get()->default_handler(vcpu, &guest_ctx);
            break;
        case vmexit::xsetbv: {
            xsetbv_handler::get()->xsetbv_exit_handler(vcpu, &guest_ctx);
            break;
        }
        case vmexit::invalid: {
            vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
            break;
        }
        default: {
            KeBugCheck(0xB16B00B5UL);
        }
    }

    if (guest_ctx.should_exit) {
        // Load guest CR3 context
        __writecr3(vcpu->guest_vmcb.state_save_area.cr3.value);

        //  RBX     : address to return
        //  RCX     : stack pointer to restore
        //  EDX:EAX : address of per processor data to be freed by the caller
        guest_ctx.vprocessor_registers->rax.value = reinterpret_cast<uint64_t>(vcpu) & MAXUINT;
        guest_ctx.vprocessor_registers->rbx.value = vcpu->guest_vmcb.control_area.next_rip;
        guest_ctx.vprocessor_registers->rcx.value = vcpu->guest_vmcb.state_save_area.rsp;
        guest_ctx.vprocessor_registers->rdx.value = reinterpret_cast<uint64_t>(vcpu) >> 32;

        // load guest state
        __svm_vmload(MmGetPhysicalAddress(&vcpu->guest_vmcb).QuadPart);

        // set the global interrupt flag (GIF), but still disable interrupts by
        // clearing IF. GIF must be set to return to the normal execution, but
        // interruptions are unwanted untill SVM is disabled as it would
        // execute random kernel-code in the host context.
        _disable();
        __svm_stgi();

        // disable svm and restore the guest rflags
        // this may enable interrupts
        __writemsr(IA32_MSR_EFER,
            __readmsr(IA32_MSR_EFER) & ~EFER_SVME);

        __writeeflags(vcpu->guest_vmcb.state_save_area.rflags);
        NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);
        return guest_ctx.should_exit;
    }

    vcpu->guest_vmcb.state_save_area.rax = guest_ctx.vprocessor_registers->rax.value;
    NT_ASSERT(vcpu->host_stack_layout.reserved1 == MAXUINT64);

    return guest_ctx.should_exit;
}
