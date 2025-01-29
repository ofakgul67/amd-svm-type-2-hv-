#pragma once
#include "../includes.h"

class xsetbv_handler {
private:
    bool is_valid_xcr0(u64 value);

public:
    static xsetbv_handler* get();
    void xsetbv_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
};