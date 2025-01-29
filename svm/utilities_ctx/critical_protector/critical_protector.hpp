#pragma once
#include "../includes.h"

class npt;

class critical_protector {
private:
    KSPIN_LOCK protector_lock;
    npt* npt_instance;

    // Lock management
    void acquire_lock(PKIRQL old_irql);
    void release_lock(KIRQL old_irql);

    // Protection helper
    void protect_range(
        svm::pvcpu_t vcpu,
        void* virtual_address,
        size_t size,
        bool allow_write,
        bool allow_exec
    );

    // Private constructor for singleton
    critical_protector();

    // Prevent copying and assignment
    critical_protector(const critical_protector&) = delete;
    critical_protector& operator=(const critical_protector&) = delete;

public:
    // Main protection function
    void protect_critical_structures(svm::pvcpu_t vcpu);

    // Singleton access
    static critical_protector* get();
};