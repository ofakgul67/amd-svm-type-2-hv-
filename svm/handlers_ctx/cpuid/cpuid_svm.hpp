#pragma once
#include "../includes.h"

struct cpuid_registers_t {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
};

class cpuid_handler {
private:
    struct config_t {
        bool stealth_mode;
        bool hide_features;
    } config;

    u8 get_current_cpu();
    cpuid_registers_t get_raw_cpuid(u32 leaf, u32 subleaf);
    void handle_basic_leaf(u32 leaf, u32 subleaf, cpuid_registers_t& regs);
    void handle_extended_leaf(u32 leaf, u32 subleaf, cpuid_registers_t& regs);

public:
    static cpuid_handler* get();
    void cpuid_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void initialize();
};