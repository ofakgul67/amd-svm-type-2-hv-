#pragma once
#include "../includes.h"
#pragma warning(disable : 6385)

struct hook_context {
    void* original_address;
    void* hook_address;
    size_t hook_size;
    u64 original_pfn;
    u64 hook_pfn;
    bool is_active;
    PMDL original_mdl;
    PMDL hook_mdl;
};

class hook {
private:
    static constexpr size_t MAX_HOOKS = 128;
    hook_context hooks[MAX_HOOKS];
    volatile size_t hook_count;
    KSPIN_LOCK hook_lock;
    auto lock_pages(void* address, size_t size, PMDL* out_mdl) -> bool;
    auto unlock_pages(PMDL mdl) -> void;

public:
    hook();
    auto install_function_hook(svm::pvcpu_t vcpu, void* target, void* hook_func, void** original) -> svm::svmroot_error_t;
    auto remove_function_hook(svm::pvcpu_t vcpu, void* target) -> svm::svmroot_error_t;
    static auto get() -> hook*;
};