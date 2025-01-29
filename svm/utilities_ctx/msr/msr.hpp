#pragma once
#include "../includes.h"

class msrs {
public:
    struct msr_range {
        u32 start;
        u32 end;
    };

    enum amd_msrs {
        // Debug and System MSRs
        TSC = 0x10,
        DEBUG_CTL = 0x1D9,
        PAT = 0x277,

        // SYSENTER/SYSCALL MSRs
        SYSENTER_CS = 0x174,
        SYSENTER_ESP = 0x175,
        SYSENTER_EIP = 0x176,
        LSTAR = 0xC0000082,
        CSTAR = 0xC0000083,
        SFMASK = 0xC0000084,

        // SVM MSRs
        EFER = 0xC0000080,
        VM_HSAVE_PA = 0xC0010117,

        // Performance MSRs
        PERFEVTSEL0 = 0xC0010000,
        PERFEVTSEL1 = 0xC0010001,
        PERFEVTSEL2 = 0xC0010002,
        PERFEVTSEL3 = 0xC0010003,
        PERF_CTR0 = 0xC0010004,
        PERF_CTR1 = 0xC0010005,
        PERF_CTR2 = 0xC0010006,
        PERF_CTR3 = 0xC0010007,

        // System Config MSRs
        HWCR = 0xC0010015,
        VM_CR = 0xC0010114,
        K8_SYSCFG = 0xC0010010,

        // LBR MSRs
        MSR_LASTBRANCHFROM_IP = 0x1DB,
        MSR_LASTBRANCHTO_IP = 0x1DC,
        MSR_LASTEXCPFROM_IP = 0x1DD,
        MSR_LASTEXCPTO_IP = 0x1DE,
    };

    auto prepare_msrpm(msrpm_t& msrpm) -> svm::svmroot_error_t;
    static auto get() -> msrs*;

private:
    void setup_svm_msrs(msrpm_t& msrpm);
    void setup_perf_msrs(msrpm_t& msrpm);
    void setup_syscfg_msrs(msrpm_t& msrpm);
    void setup_lbr_msrs(msrpm_t& msrpm);
    void setup_debug_msrs(msrpm_t& msrpm);
    void setup_system_msrs(msrpm_t& msrpm);
};