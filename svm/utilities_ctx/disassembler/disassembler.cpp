#include "disassembler.hpp"

disassembler::disassembler() noexcept {
    ZydisDecoderInit(&zy_dec64, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    ZydisDecoderInit(&zy_dec32, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
    ZydisDecoderInit(&zy_dec16, ZYDIS_MACHINE_MODE_REAL_16, ZYDIS_STACK_WIDTH_16);
    ZydisFormatterInit(&zy_fmt, ZYDIS_FORMATTER_STYLE_INTEL);
}

disassembler* disassembler::get() {
    static disassembler instance;
    return &instance;
}

uint32_t disassembler::decode_length(void* code, uint8_t arch_bits) {
    switch (arch_bits) {
    case 16: return decode_length_x16(static_cast<PBYTE>(code), 0);
    case 32: return decode_length_x32(static_cast<PBYTE>(code), 0);
    case 64: return decode_length_x64(static_cast<PBYTE>(code), 0);
    default: return 0;
    }
}

ZyanU8 disassembler::decode_length_x16(ZyanU8* code, ZyanUSize code_length) {
    ZydisDecodedInstruction zy_ins;
    ZydisDecoderDecodeInstruction(
        &zy_dec16,
        static_cast<ZydisDecoderContext*>(ZYAN_NULL),
        code,
        code_length == 0 ? 15 : code_length,
        &zy_ins
    );
    return zy_ins.length;
}

ZyanU8 disassembler::decode_length_x32(ZyanU8* code, ZyanUSize code_length) {
    ZydisDecodedInstruction zy_ins;
    ZydisDecoderDecodeInstruction(
        &zy_dec32,
        static_cast<ZydisDecoderContext*>(ZYAN_NULL),
        code,
        code_length == 0 ? 15 : code_length,
        &zy_ins
    );
    return zy_ins.length;
}

ZyanU8 disassembler::decode_length_x64(ZyanU8* code, ZyanUSize code_length) {
    ZydisDecodedInstruction zy_ins;
    ZydisDecoderDecodeInstruction(
        &zy_dec64,
        static_cast<ZydisDecoderContext*>(ZYAN_NULL),
        code,
        code_length == 0 ? 15 : code_length,
        &zy_ins
    );
    return zy_ins.length;
}

uint32_t disassembler::decode_instruction(void* code, char* mnemonic, size_t mnemonic_length, uint8_t arch_bits, uint64_t virtual_address) {
    switch (arch_bits) {
    case 16: return decode_instruction_x16(mnemonic, mnemonic_length, static_cast<PBYTE>(code), 15, virtual_address);
    case 32: return decode_instruction_x32(mnemonic, mnemonic_length, static_cast<PBYTE>(code), 15, virtual_address);
    case 64: return decode_instruction_x64(mnemonic, mnemonic_length, static_cast<PBYTE>(code), 15, virtual_address);
    default: return 0;
    }
}

ZyanU8 disassembler::decode_instruction_x64(char* mnemonic, ZyanUSize mnemonic_length, ZyanU8* code, ZyanUSize code_length, ZyanU64 virtual_address) {
    ZydisDecodedInstruction zy_ins;
    ZydisDecodedOperand zy_ops[ZYDIS_MAX_OPERAND_COUNT];

    if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&zy_dec64, code, code_length, &zy_ins, zy_ops))) {
        ZydisFormatterFormatInstruction(
            &zy_fmt,
            &zy_ins,
            zy_ops,
            zy_ins.operand_count_visible,
            mnemonic,
            mnemonic_length,
            virtual_address,
            ZYAN_NULL
        );
    }
    return zy_ins.length;
}

ZyanU8 disassembler::decode_instruction_x32(char* mnemonic, ZyanUSize mnemonic_length, ZyanU8* code, ZyanUSize code_length, ZyanU64 virtual_address) {
    ZydisDecodedInstruction zy_ins;
    ZydisDecodedOperand zy_ops[ZYDIS_MAX_OPERAND_COUNT];

    if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&zy_dec32, code, code_length, &zy_ins, zy_ops))) {
        ZydisFormatterFormatInstruction(
            &zy_fmt,
            &zy_ins,
            zy_ops,
            zy_ins.operand_count_visible,
            mnemonic,
            mnemonic_length,
            virtual_address,
            ZYAN_NULL
        );
    }
    return zy_ins.length;
}

ZyanU8 disassembler::decode_instruction_x16(char* mnemonic, ZyanUSize mnemonic_length, ZyanU8* code, ZyanUSize code_length, ZyanU64 virtual_address) {
    ZydisDecodedInstruction zy_ins;
    ZydisDecodedOperand zy_ops[ZYDIS_MAX_OPERAND_COUNT];

    if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&zy_dec16, code, code_length, &zy_ins, zy_ops))) {
        ZydisFormatterFormatInstruction(
            &zy_fmt,
            &zy_ins,
            zy_ops,
            zy_ins.operand_count_visible,
            mnemonic,
            mnemonic_length,
            virtual_address,
            ZYAN_NULL
        );
    }
    return zy_ins.length;
}
