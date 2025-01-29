#pragma once
#include "../includes.h"

class rdtsc_handler {
public:
    // Singleton access
    static rdtsc_handler* get();

    // Handler for RDTSC VM exits
    void rdtsc_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);

private:
    // Prevent direct construction
    rdtsc_handler() = default;

    // Maximum number of supported CPUs
    static constexpr u8 MAX_CPUS = 64;

    // Per-CPU data structure for TSC tracking
    struct per_cpu_data {
        u64 rdtsc_fake = 0;     // Spoofed TSC value
        u64 rdtsc_prev = 0;     // Previous real TSC value
        u64 last_calibration = 0; // Last calibration value for randomization
    };

    // Array to store per-CPU TSC data
    per_cpu_data cpu_data[MAX_CPUS];

    // Get current CPU index
    u8 get_current_cpu();
};