#pragma once
#include "../includes.h"

#define DEBUGCTL_LBR (1ULL << 0)  // Last Branch Record
#define DEBUGCTL_BTF (1ULL << 1)  // Branch Trace Flag

#define DEBUGCTL_RESERVED_BITS (~(0x3fULL))

enum amd_msrs {
    // SVM MSRs
    EFER = 0xC0000080,
    VM_HSAVE_PA = 0xC0010117,

    // Debug and System MSRs
    DEBUG_CTL = 0x1D9,
    TSC = 0x10,
    PAT = 0x277,
    LSTAR = 0xC0000082,
    CSTAR = 0xC0000083,
    SFMASK = 0xC0000084,

    // Performance MSRs
    PERF_CTR0 = 0xC0010004,
    PERF_CTR1 = 0xC0010005,
    PERF_CTR2 = 0xC0010006,
    PERF_CTR3 = 0xC0010007,
    PERFEVTSEL0 = 0xC0010000,
    PERFEVTSEL1 = 0xC0010001,
    PERFEVTSEL2 = 0xC0010002,
    PERFEVTSEL3 = 0xC0010003,

    // System Config MSRs
    HWCR = 0xC0010015,
    VM_CR = 0xC0010114,
    K8_SYSCFG = 0xC0010010,

    // SYSENTER MSRs
    SYSENTER_CS = 0x174,
    SYSENTER_ESP = 0x175,
    SYSENTER_EIP = 0x176,

    // LBR MSRs
    MSR_LASTBRANCHFROM_IP = 0x1DB,
    MSR_LASTBRANCHTO_IP = 0x1DC,
    MSR_LASTEXCPFROM_IP = 0x1DD,
    MSR_LASTEXCPTO_IP = 0x1DE,

    // Performance Monitoring MSRs
    IA32_APERF_MSR = 0xE8,
    IA32_MPERF_MSR = 0xE7,
};

class msr_handler {
private:
    struct msr_cache_entry {
        u64 value;
        bool initialized;
    };

    struct perf_counter_state {
        u64 start_tsc;
        u64 last_value;
        u64 increment_rate;
        u64 event_select;
        bool enabled;
    };

    struct per_cpu_data {
        msr_cache_entry cached_msrs[0x1000];
        perf_counter_state perf_counters[4];
    };

    struct per_cpu_tsc_data {
        u64 fake_tsc;
        u64 last_real;
        u64 aperf_value;
        u64 mperf_value;
        bool initialized;
    };

    struct msr_config {
        bool cache_enabled;
        bool virtualize_performance_msrs;
        bool stealth_mode;
        bool maintain_branch_history;
    } config;

    per_cpu_data cpu_data[64];
    per_cpu_tsc_data tsc_data[64] = {};

    void initialize_cpu_data();
    void handle_msr_read(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value);
    void handle_virtualization_msrs(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value);
    void handle_tsc_msr(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value, u8 cpu_index);
    void handle_perf_msrs(svm::pvcpu_t vcpu, u32 msr, ULARGE_INTEGER& value, u8 cpu_index);

public:
    static msr_handler* get();
    u8 get_current_cpu();
    void wrmsr_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, u32 msr);
    void rdmsr_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, u32 msr);
    void msr_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void initialize();
};