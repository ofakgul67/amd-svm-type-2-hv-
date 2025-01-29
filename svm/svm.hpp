#pragma once
#include "../includes.h"

class virtualization {
private:
    static inline bool initialized;
public:
    static void virtualize();
    static void devirtualize();

    static void setup_control_fields(svm::pvcpu_t vcpu);
    static void setup_guest(svm::pvcpu_t vcpu, const svm::pcontext_t ctx);
    static void setup_host(svm::pvcpu_t vcpu);
    static bool virtualize_processor(PVOID ctx);
    static bool devirtualize_processor(PVOID ctx);
};
