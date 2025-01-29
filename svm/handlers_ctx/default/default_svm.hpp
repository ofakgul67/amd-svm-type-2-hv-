#pragma once
#include "../includes.h"

class svm_handler {
public:
    // Get singleton instance
    static auto get() -> svm_handler*;

    // Primary handler for SVM instructions
    void default_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);

    // Initialization
    void initialize();

private:
    // Configuration flags
    struct {
        bool enforce_svm_lock{ true };
        bool prevent_nested_virtualization{ true };
        bool log_svm_exits{ false };
    } config;

    // Handler for specific SVM instructions
    void handle_mwait(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_mwait_conditional(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_monitor(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_vmrun(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_vmload_vmsave(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_clgi_stgi(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_skinit(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_rdtscp(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_icebp(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
    void handle_invlpga(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);

    // Utility functions
    void advance_rip(svm::pvcpu_t vcpu);
    bool check_svm_enabled(svm::pvcpu_t vcpu);
    void inject_invalid_opcode(svm::pvcpu_t vcpu);

    // Private constructor/destructor for singleton
    svm_handler() = default;
    ~svm_handler() = default;
    svm_handler(const svm_handler&) = delete;
    svm_handler& operator=(const svm_handler&) = delete;
};