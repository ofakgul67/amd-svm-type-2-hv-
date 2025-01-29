#include "msr_svm.hpp"

msr_handler* msr_handler::get() {
    static msr_handler instance;
    static bool is_initialized = false;

    if (!is_initialized) {
        instance.initialize();
        is_initialized = true;
    }
    return &instance;
}

void msr_handler::initialize() {
    initialize_cpu_data();
}

u8 msr_handler::get_current_cpu() {
    return static_cast<u8>(KeGetCurrentProcessorNumber());
}

void msr_handler::initialize_cpu_data() {
    RtlZeroMemory(cpu_data, sizeof(cpu_data));

    config.cache_enabled = true;
    config.virtualize_performance_msrs = true;
    config.stealth_mode = true;
    config.maintain_branch_history = true;

    for (int cpu = 0; cpu < 64; cpu++) {
        for (int i = 0; i < 4; i++) {
            cpu_data[cpu].perf_counters[i].start_tsc = __rdtsc();
            cpu_data[cpu].perf_counters[i].last_value = 0;
            cpu_data[cpu].perf_counters[i].increment_rate = 1 << (7 + i);
            cpu_data[cpu].perf_counters[i].event_select = 0;
            cpu_data[cpu].perf_counters[i].enabled = false;
        }
    }
}

void msr_handler::handle_tsc_msr(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value, u8 cpu_index) {
    // Per-CPU TSC data to maintain individual state
    static u64 per_cpu_rdtsc_fake[64] = { 0 };
    static u64 per_cpu_rdtsc_prev[64] = { 0 };
    static u64 per_cpu_last_random[64] = { 0 };

    // Validate CPU index
    if (cpu_index >= 64) {
        value.QuadPart = 0;
        return;
    }

    u64& rdtsc_fake = per_cpu_rdtsc_fake[cpu_index];
    u64& rdtsc_prev = per_cpu_rdtsc_prev[cpu_index];
    u64& last_random = per_cpu_last_random[cpu_index];

    // Initialize if first read
    if (rdtsc_prev == 0) {
        rdtsc_prev = __rdtsc();
        rdtsc_fake = rdtsc_prev;
    }

    u64 rdtsc_real = __rdtsc();

    // Ensure progression and simulate realistic TSC behavior
    if (rdtsc_real > rdtsc_prev) {
        u64 diff = rdtsc_real - rdtsc_prev;

        // Base increment calculation
        u64 base_increment = diff / 20;

        // Generate a random increment between 25 and 500
        // Use last_random and current rdtsc to create pseudo-random value
        u64 random_increment = 25 + ((base_increment ^ last_random ^ rdtsc_real) % 476);
        last_random = random_increment;

        rdtsc_fake += random_increment;
    }

    // Never exceed real TSC to maintain credibility
    if (rdtsc_fake > rdtsc_real) {
        rdtsc_fake = rdtsc_real;
    }

    // Update tracking values
    rdtsc_prev = rdtsc_real;

    // Set return value
    value.QuadPart = rdtsc_fake;
}

void msr_handler::handle_perf_msrs(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value, u8 cpu_index) {
    static u64 per_cpu_perf_fake[64] = { 0 };
    static u64 per_cpu_perf_prev[64] = { 0 };

    if (cpu_index >= 64) {
        value.QuadPart = 0;
        return;
    }

    u64& perf_fake = per_cpu_perf_fake[cpu_index];
    u64& perf_prev = per_cpu_perf_prev[cpu_index];

    u64 rdtsc_real = __rdtsc();

    if (perf_prev == 0) {
        perf_prev = rdtsc_real;
        perf_fake = rdtsc_real & 0xFF;
    }

    if (rdtsc_real > perf_prev) {
        // Increment counter between reads
        perf_fake += 200 + (rdtsc_real & 0x3F);
    }

    perf_prev = rdtsc_real;
    value.QuadPart = perf_fake;
}

void msr_handler::handle_msr_read(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value) {
    u8 cpu_index = get_current_cpu();

    // Handle known MSRs
    switch (msr) {
    case PAT: 
        value.QuadPart = vcpu->guest_vmcb.state_save_area.gpat;
        break;

    case LSTAR: 
        value.QuadPart = vcpu->guest_vmcb.state_save_area.lstar;
        break;

    case CSTAR:
        value.QuadPart = vcpu->guest_vmcb.state_save_area.cstar;
        break;

    case SFMASK:
        value.QuadPart = vcpu->guest_vmcb.state_save_area.sfmask;
        break;

    case SYSENTER_CS:
        value.QuadPart = vcpu->guest_vmcb.state_save_area.sysenter_cs;
        break;

    case SYSENTER_ESP:
        value.QuadPart = vcpu->guest_vmcb.state_save_area.sysenter_esp;
        break;

    case SYSENTER_EIP:
        value.QuadPart = vcpu->guest_vmcb.state_save_area.sysenter_eip;
        break;
    }
}

void msr_handler::handle_virtualization_msrs(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value) {
    switch (msr) {
    case EFER:
        value.QuadPart = vcpu->guest_vmcb.state_save_area.efer.value;
        break;

    case VM_CR:
        value.QuadPart = __readmsr(VM_CR);
        break;

    case VM_HSAVE_PA:
        value.QuadPart = vcpu->host_stack_layout.shadow_data.hsave_pa.value; 
        break;

    case K8_SYSCFG:
        value.QuadPart = __readmsr(K8_SYSCFG);
        break;

    case HWCR:
        value.QuadPart = __readmsr(HWCR);
        break;
    }
}

void msr_handler::msr_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u32 msr = guest_ctx->vprocessor_registers->rcx.value & MAXUINT;
    bool write_access = (vcpu->guest_vmcb.control_area.exit_info1 != 0);

    if (write_access) {
        wrmsr_handler(vcpu, guest_ctx, msr);
    }
    else {
        rdmsr_handler(vcpu, guest_ctx, msr);
    }

    vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
}

void msr_handler::rdmsr_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, u32 msr) {
    ULARGE_INTEGER value{};
    u8 cpu_index = get_current_cpu();
    bool handled = false;

    if (config.stealth_mode) {
        switch (msr) {
            // Timestamp Counter MSR - Used for timing and performance measurements
            case TSC: {
                handle_tsc_msr(vcpu, msr, value, cpu_index);
                break;
            }

           // Performance Monitoring MSRs - Track CPU performance metrics
            case IA32_APERF_MSR:  
            case IA32_MPERF_MSR: {  
                handle_perf_msrs(vcpu, msr, value, cpu_index);
                break;
            }

            // System Configuration MSRs - Control system behavior and features
            case PAT:          
            case LSTAR:        
            case CSTAR:        
            case SFMASK:       
            case SYSENTER_CS:  
            case SYSENTER_ESP: 
            case SYSENTER_EIP: { 
                handle_msr_read(vcpu, msr, value);
                break;
            }

            // AMD Virtualization MSRs - Control CPU virtualization features
            case EFER:          
            case VM_CR:         
            case VM_HSAVE_PA:
            case K8_SYSCFG: {   
                handle_virtualization_msrs(vcpu, msr, value);
                break;
            }

            // Debug Control MSR - Configure processor debug features
            case DEBUG_CTL:
                value.QuadPart = vcpu->guest_vmcb.state_save_area.dbg_ctl;
                break;

            case MSR_LASTBRANCHFROM_IP:
                value.QuadPart = vcpu->guest_vmcb.state_save_area.br_from;
                break;

            case MSR_LASTBRANCHTO_IP:
                value.QuadPart = vcpu->guest_vmcb.state_save_area.br_to;
                break;

            case MSR_LASTEXCPFROM_IP:
                value.QuadPart = vcpu->guest_vmcb.state_save_area.last_excep_from;
                break;

            case MSR_LASTEXCPTO_IP:
                value.QuadPart = vcpu->guest_vmcb.state_save_area.last_exep_to;
                break;

            // KVM Hypervisor MSR - Used by KVM for guest/host communication
            case 0x4B564D00:
                event_inj_t{}.general_protection_exception(vcpu);
                return;

            default:
                value.QuadPart = 0;
                break;
        }
    }

    guest_ctx->vprocessor_registers->rax.value = value.LowPart;
    guest_ctx->vprocessor_registers->rdx.value = value.HighPart;
}

void msr_handler::wrmsr_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, u32 msr) {
    ULARGE_INTEGER value{};
    value.LowPart = guest_ctx->vprocessor_registers->rax.value & MAXUINT;
    value.HighPart = guest_ctx->vprocessor_registers->rdx.value & MAXUINT;
    u8 cpu_index = get_current_cpu();

    // Validate CPU index
    if (cpu_index >= 64) {
        event_inj_t{}.general_protection_exception(vcpu);
        return;
    }

    // Performance event select registers
    if (msr >= PERFEVTSEL0 && msr <= PERFEVTSEL3) {
        u32 idx = msr - PERFEVTSEL0;
        if (idx < 4) {
            cpu_data[cpu_index].perf_counters[idx].event_select = value.QuadPart;
            cpu_data[cpu_index].perf_counters[idx].enabled = (value.QuadPart & (1ULL << 22)) != 0;
        }
        else {
            event_inj_t{}.general_protection_exception(vcpu);
        }
        return;
    }

    switch (msr) {
        // Time Stamp Counter
        case TSC: {
            __writemsr(msr, value.QuadPart);
            break;
        }

        // Performance MSRs
        case IA32_APERF_MSR:
        case IA32_MPERF_MSR: {
            __writemsr(msr, value.QuadPart);
            break;
        }

        // Sysenter MSRs
        case SYSENTER_CS:
        case SYSENTER_ESP:
        case SYSENTER_EIP: {
            __writemsr(msr, value.QuadPart);
            break;
        }

        // Virtualization MSRs with special handling
        case EFER: {
            auto& shadow_efer = vcpu->host_stack_layout.shadow_data.efer;

            // Only block SVME bit
            if (value.QuadPart & (1ULL << 12)) {
                event_inj_t{}.general_protection_exception(vcpu);
                return;
            }

            // Update both VMCB and shadow
            vcpu->guest_vmcb.state_save_area.efer.value = value.QuadPart;
            shadow_efer.value = value.QuadPart;
            break;
        }

        case VM_HSAVE_PA: {
            vcpu->host_stack_layout.shadow_data.hsave_pa.value = value.QuadPart;
            break;
        }

        case DEBUG_CTL:
            // First check for reserved bits
            if (value.QuadPart & DEBUGCTL_RESERVED_BITS) {
                event_inj_t{}.general_protection_exception(vcpu);
                return;
            }

            // Store the value in VMCB (masking off LBR and BTF as before)
            vcpu->guest_vmcb.state_save_area.dbg_ctl = value.QuadPart;

            // If LBR bit is being enabled
            if (value.QuadPart & DEBUGCTL_LBR) {
                // Enable LBR virtualization in VMCB
                vcpu->guest_vmcb.control_area.lbr_virtualization_enable = 1;

                // Store current branch information
                vcpu->guest_vmcb.state_save_area.br_from = __readmsr(MSR_LASTBRANCHFROM_IP);
                vcpu->guest_vmcb.state_save_area.br_to = __readmsr(MSR_LASTBRANCHTO_IP);
                vcpu->guest_vmcb.state_save_area.last_excep_from = __readmsr(MSR_LASTEXCPFROM_IP);
                vcpu->guest_vmcb.state_save_area.last_exep_to = __readmsr(MSR_LASTEXCPTO_IP);
            }
            else {
                // Disable LBR virtualization
                vcpu->guest_vmcb.control_area.lbr_virtualization_enable = 0;

                // Store current branch information
                vcpu->guest_vmcb.state_save_area.br_from = 0;
                vcpu->guest_vmcb.state_save_area.br_to = 0;
                vcpu->guest_vmcb.state_save_area.last_excep_from = 0;
                vcpu->guest_vmcb.state_save_area.last_exep_to = 0;
            }
            break;

        case MSR_LASTBRANCHFROM_IP:
            vcpu->guest_vmcb.state_save_area.br_from = value.QuadPart;
            break;

        case MSR_LASTBRANCHTO_IP:
            vcpu->guest_vmcb.state_save_area.br_to = value.QuadPart;
            break;

        case MSR_LASTEXCPFROM_IP:
            vcpu->guest_vmcb.state_save_area.last_excep_from = value.QuadPart;
            break;

        case MSR_LASTEXCPTO_IP:
            vcpu->guest_vmcb.state_save_area.last_exep_to = value.QuadPart;
            break;

        case PAT: {
            __writemsr(msr, value.QuadPart);
            break;
        }

        case LSTAR:
        case CSTAR:
        case SFMASK: {
            __writemsr(msr, value.QuadPart);
            break;
        }

        default: {
            __writemsr(msr, value.QuadPart);
            break;
        }
    }
}