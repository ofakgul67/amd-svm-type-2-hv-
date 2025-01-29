#pragma once
#include "../includes.h"

struct instruction_info {
    u8* rip;           // Instruction pointer
    u32 length;        // Instruction length
    u8 opcode;         // Primary opcode
    u8 modrm;          // ModR/M byte if present
    bool has_modrm;    // Has ModR/M byte
    bool has_prefix;   // Has prefix bytes
    u8 prefix[4];      // Prefix bytes (max 4)
    u8 prefix_count;   // Number of prefixes
};

class ud_handler {
private:
    bool decode_instruction(svm::pvcpu_t vcpu, instruction_info& info);
    bool emulate_instruction(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, instruction_info& info);
    bool handle_privileged_instruction(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, instruction_info& info);
    void advance_rip(svm::pvcpu_t vcpu, u32 instruction_length);

public:
    static ud_handler* get();
    void ud_exception_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
};