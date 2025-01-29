#include "ud_svm.hpp"

ud_handler* ud_handler::get() {
    static ud_handler instance;
    return &instance;
}

bool ud_handler::decode_instruction(svm::pvcpu_t vcpu, instruction_info& info) {
    info.rip = reinterpret_cast<u8*>(vcpu->guest_vmcb.state_save_area.rip);
    info.length = 0;
    info.has_modrm = false;
    info.has_prefix = false;
    info.prefix_count = 0;

    u8 instruction_bytes[15] = { 0 };

    auto status = memory_access::get()->read_virtual_memory(
        vcpu,
        vcpu->guest_vmcb.state_save_area.cr3,
        reinterpret_cast<u64>(instruction_bytes),
        reinterpret_cast<u64>(info.rip),
        sizeof(instruction_bytes)
    );

    if (status != svm::svmroot_error_t::error_success) {
        return false;
    }

    u8* current = instruction_bytes;

    // Handle legacy prefixes
    while (*current == 0xF0 || *current == 0xF2 || *current == 0xF3) {
        if (info.prefix_count < 4) {
            info.prefix[info.prefix_count++] = *current;
            info.has_prefix = true;
        }
        current++;
        info.length++;
        if (info.length >= 15) return false;
    }

    // Get opcode 
    info.opcode = *current++;
    info.length++;

    // Check if instruction has ModR/M byte based on opcode
    if ((info.opcode >= 0x80 && info.opcode <= 0x83) ||
        (info.opcode >= 0x88 && info.opcode <= 0x8B) ||
        info.opcode == 0xC6 || info.opcode == 0xC7 ||
        info.opcode == 0x0F)
    {
        info.has_modrm = true;
        info.modrm = *current;
        info.length++;
    }

    return true;
}

bool ud_handler::handle_privileged_instruction(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, instruction_info& info) {
    switch (info.opcode) {
        case 0x0F: { // Two-byte opcode
            u8 instruction_bytes[2] = { 0 };
            auto status = memory_access::get()->read_virtual_memory(
                vcpu,
                vcpu->guest_vmcb.state_save_area.cr3,
                reinterpret_cast<u64>(instruction_bytes),
                reinterpret_cast<u64>(info.rip),
                sizeof(instruction_bytes)
            );

            if (status != svm::svmroot_error_t::error_success) {
                return false;
            }

            u8 second_byte = instruction_bytes[1];

            // Handle specific two-byte opcodes
            switch (second_byte) {
                case 0x01: // Check for VMCALL/VMMCALL
                    if (info.modrm == 0xC1) {
                        advance_rip(vcpu, info.length);
                        return true;
                    }
                    break;
            }
            break;
        }

        case 0x0B: // UD2 instruction
            advance_rip(vcpu, info.length);
            return true;
    }

    return false;
}

bool ud_handler::emulate_instruction(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, instruction_info& info) {
    return handle_privileged_instruction(vcpu, guest_ctx, info);
}

void ud_handler::advance_rip(svm::pvcpu_t vcpu, u32 instruction_length) {
    vcpu->guest_vmcb.state_save_area.rip += instruction_length;
}

void ud_handler::ud_exception_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    instruction_info info{};

    // Try to decode and emulate the instruction
    if (decode_instruction(vcpu, info)) {
        if (emulate_instruction(vcpu, guest_ctx, info)) {
            return;
        }
    }

    // If emulation fails, inject #UD back to guest
    event_inj_t{}.invalid_opcode_exception(vcpu);
}