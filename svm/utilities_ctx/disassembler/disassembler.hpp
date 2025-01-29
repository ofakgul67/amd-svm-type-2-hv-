#pragma once
#include "../includes.h"

class disassembler {
private:
    ZydisDecoder zy_dec16;
    ZydisDecoder zy_dec32;
    ZydisDecoder zy_dec64;
    ZydisFormatter zy_fmt;

    // Private constructor for singleton pattern
    disassembler() noexcept;

    // Helper methods for specific architectures
    ZyanU8 decode_length_x16(ZyanU8* code, ZyanUSize code_length);
    ZyanU8 decode_length_x32(ZyanU8* code, ZyanUSize code_length);
    ZyanU8 decode_length_x64(ZyanU8* code, ZyanUSize code_length);

    ZyanU8 decode_instruction_x16(char* mnemonic, ZyanUSize mnemonic_length, ZyanU8* code, ZyanUSize code_length, ZyanU64 virtual_address);
    ZyanU8 decode_instruction_x32(char* mnemonic, ZyanUSize mnemonic_length, ZyanU8* code, ZyanUSize code_length, ZyanU64 virtual_address);
    ZyanU8 decode_instruction_x64(char* mnemonic, ZyanUSize mnemonic_length, ZyanU8* code, ZyanUSize code_length, ZyanU64 virtual_address);

public:
    // Delete copy constructor and assignment operator
    disassembler(const disassembler&) = delete;
    disassembler& operator=(const disassembler&) = delete;

    // Singleton instance getter
    static disassembler* get();

    // Public interface
    uint32_t decode_length(void* code, uint8_t arch_bits);
    uint32_t decode_instruction(void* code, char* mnemonic, size_t mnemonic_length, uint8_t arch_bits, uint64_t virtual_address);
};
