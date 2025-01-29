#include "msr.hpp"

void msrs::setup_debug_msrs(msrpm_t& msrpm) {
    msrpm.set(DEBUG_CTL, access::read);
    msrpm.set(DEBUG_CTL, access::write);
    msrpm.set(TSC, access::read);
    msrpm.set(TSC, access::write);
}

void msrs::setup_system_msrs(msrpm_t& msrpm) {
    // System MSRs
    msrpm.set(PAT, access::read);
    msrpm.set(PAT, access::write);
    msrpm.set(LSTAR, access::read);
    msrpm.set(LSTAR, access::write);
    msrpm.set(CSTAR, access::read);
    msrpm.set(CSTAR, access::write);
    msrpm.set(SFMASK, access::read);
    msrpm.set(SFMASK, access::write);

    // SYSENTER MSRs - Add write interception
    msrpm.set(SYSENTER_CS, access::read);
    msrpm.set(SYSENTER_CS, access::write);    
    msrpm.set(SYSENTER_ESP, access::read);
    msrpm.set(SYSENTER_ESP, access::write);
    msrpm.set(SYSENTER_EIP, access::read);
    msrpm.set(SYSENTER_EIP, access::write);
}

void msrs::setup_perf_msrs(msrpm_t& msrpm) {
    // MPERF/APERF interception
    msrpm.set(IA32_MPERF_MSR, access::read);
    msrpm.set(IA32_MPERF_MSR, access::write);
    msrpm.set(IA32_APERF_MSR, access::read);
    msrpm.set(IA32_APERF_MSR, access::write);
}

void msrs::setup_svm_msrs(msrpm_t& msrpm) {
    // Essential SVM MSRs must be intercepted
    msrpm.set(EFER, access::read);
    msrpm.set(EFER, access::write);
    msrpm.set(VM_HSAVE_PA, access::read);
    msrpm.set(VM_HSAVE_PA, access::write);
}

void msrs::setup_syscfg_msrs(msrpm_t& msrpm) {
    // System configuration MSRs
    msrpm.set(HWCR, access::read);
    msrpm.set(HWCR, access::write);
    msrpm.set(VM_CR, access::read);
    msrpm.set(VM_CR, access::write);
    msrpm.set(K8_SYSCFG, access::read);
    msrpm.set(K8_SYSCFG, access::write);
}

void msrs::setup_lbr_msrs(msrpm_t& msrpm) {
    // Intercept Last Branch Record MSRs
    msrpm.set(MSR_LASTBRANCHFROM_IP, access::read);
    msrpm.set(MSR_LASTBRANCHFROM_IP, access::write);
    msrpm.set(MSR_LASTBRANCHTO_IP, access::read);
    msrpm.set(MSR_LASTBRANCHTO_IP, access::write);
    msrpm.set(MSR_LASTEXCPFROM_IP, access::read);
    msrpm.set(MSR_LASTEXCPFROM_IP, access::write);
    msrpm.set(MSR_LASTEXCPTO_IP, access::read);
    msrpm.set(MSR_LASTEXCPTO_IP, access::write);
}

auto msrs::prepare_msrpm(msrpm_t& msrpm) -> svm::svmroot_error_t {
    RtlZeroMemory(&msrpm, sizeof(msrpm));

    // Setup MSR interceptions
    setup_svm_msrs(msrpm);
    setup_system_msrs(msrpm);
    setup_perf_msrs(msrpm);
    setup_syscfg_msrs(msrpm);
    setup_lbr_msrs(msrpm);
    setup_debug_msrs(msrpm);    

    return svm::svmroot_error_t::error_success;
}

auto msrs::get() -> msrs* {
    static msrs instance;
    return &instance;
}
