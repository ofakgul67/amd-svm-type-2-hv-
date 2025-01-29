#pragma once
#include "../includes.h"

class cr_handler {
private:
    struct cr_config {
        bool enforce_smep;       // Force SMEP to be enabled
        bool enforce_smap;       // Force SMAP to be enabled
        bool track_cr3;          // Track CR3 operations
        bool shadow_cr4;         // Maintain shadow CR4 value
    } config;

    struct per_cpu_state {
        bool initialized;
        u64 shadow_cr3;
        u64 shadow_cr4;
        u64 cr3_switches;
        u64 last_cr3_switch_time;
    } cpu_state[64];

    void initialize();
    void init_shadow_state(u8 cpu_index);
    u8 get_current_cpu();

    // Handler implementations
    bool handle_cr3_read(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    bool handle_cr3_write(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    bool handle_cr4_read(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    bool handle_cr4_write(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    u64* get_register_for_cr_exit(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);

public:
    static cr_handler* get();
    bool cr_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
};
