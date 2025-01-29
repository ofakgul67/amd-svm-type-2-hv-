#include "tpm.hpp"

tpm_handler* tpm_handler::get() {
    static tpm_handler instance;
    return &instance;
}

bool tpm_handler::initialize(svm::pvcpu_t vcpu) {
    KeInitializeSpinLock(&tpm_lock);
    memset(&state, 0, sizeof(state));
    generate_fake_ek();

    // Set up NPT hooks for TPM MMIO region
    return setup_tpm_hooks(vcpu);
}

void tpm_handler::cleanup(svm::pvcpu_t vcpu) {
    KIRQL old_irql;
    acquire_lock(&old_irql);

    // Remove NPT hooks
    remove_tpm_hooks(vcpu);

    memset(&state, 0, sizeof(state));
    release_lock(old_irql);
}

bool tpm_handler::setup_tpm_hooks(svm::pvcpu_t vcpu) {
    if (!vcpu) return false;

    // Calculate the number of pages to hook for TPM MMIO region
    uint64_t start_page = TPM_LOCALITY_0_BASE >> 12;
    uint64_t page_count = TPM_LOCALITY_SIZE >> 12;

    // Allocate executable pages for shadow copy
    PVOID executable_pages = ExAllocatePoolWithTag(NonPagedPool, page_count * PAGE_SIZE, 'TPMH');
    if (!executable_pages) return false;

    // Copy original TPM MMIO pages to executable pages
    memcpy(executable_pages, reinterpret_cast<void*>(TPM_LOCALITY_0_BASE), page_count * PAGE_SIZE);

    // Setup NPT hooks for each page in the TPM MMIO range
    for (uint64_t i = 0; i < page_count; i++) {
        uint64_t current_page = start_page + i;
        uint64_t executable_page = reinterpret_cast<uint64_t>(executable_pages) >> 12;

        auto status = npt::get()->install_npt_hook(
            vcpu,
            vcpu->npt[npt::nested_page_table::primary],
            vcpu->npt[npt::nested_page_table::shadow],
            current_page,
            executable_page + i
        );

        if (status != svm::svmroot_error_t::error_success) {
            // Cleanup on failure
            for (uint64_t j = 0; j < i; j++) {
                npt::get()->remove_npt_hook(
                    vcpu,
                    vcpu->npt[npt::nested_page_table::primary],
                    start_page + j
                );
            }
            ExFreePoolWithTag(executable_pages, 'TPMH');
            return false;
        }
    }

    return true;
}

bool tpm_handler::remove_tpm_hooks(svm::pvcpu_t vcpu) {
    if (!vcpu) return false;

    uint64_t start_page = TPM_LOCALITY_0_BASE >> 12;
    uint64_t page_count = TPM_LOCALITY_SIZE >> 12;

    // Remove NPT hooks for each page
    for (uint64_t i = 0; i < page_count; i++) {
        npt::get()->remove_npt_hook(
            vcpu,
            vcpu->npt[npt::nested_page_table::primary],
            start_page + i
        );
    }

    return true;
}

void tpm_handler::generate_fake_ek() {
    // Generate a randomized but consistent EK
    LARGE_INTEGER tick;
    KeQueryTickCount(&tick);

    uint32_t seed = (uint32_t)tick.LowPart;

    // Use a simple PRNG to generate deterministic but random-looking bytes
    for (int i = 0; i < sizeof(state.ek_buffer); i++) {
        seed = seed * 1103515245 + 12345;
        state.ek_buffer[i] = (uint8_t)(seed >> 16);
    }

    // Ensure the key looks valid by setting appropriate bits
    state.ek_buffer[0] |= 0x80; // Ensure first bit is 1 for proper RSA modulus
    state.ek_buffer[sizeof(state.ek_buffer) - 1] |= 0x01; // Ensure key is odd
}

bool tpm_handler::handle_tpm_mmio(svm::pvcpu_t vcpu, const page_context& ctx) {
    if (!vcpu || !is_tpm_address(ctx.guest_pa)) {
        return false;
    }

    KIRQL old_irql;
    acquire_lock(&old_irql);

    bool handled = false;
    uint64_t offset = ctx.guest_pa - TPM_LOCALITY_0_BASE;

    // Handle MMIO access based on type
    if (!ctx.is_write) {
        // Read operation
        uint32_t size = 4; // Default to 4 bytes, adjust based on access type
        handled = handle_tpm_read(vcpu, offset, size);
    }
    else {
        // Write operation
        uint32_t size = 4; // Default to 4 bytes, adjust based on access type
        uint64_t value = 0;

        // Get the value being written from guest registers or memory
        if (offset == TPM_DATA_FIFO_REG) {
            value = vcpu->guest_vmcb.state_save_area.rax;
        }

        handled = handle_tpm_write(vcpu, offset, size, value);
    }

    release_lock(old_irql);
    return handled;
}

bool tpm_handler::handle_tpm_read(svm::pvcpu_t vcpu, uint64_t offset, uint32_t size) {
    uint64_t value = 0;

    switch (offset) {
        case TPM_ACCESS_REG:
            value = 0x80; // TPM_ACCESS_VALID | TPM_ACCESS_ACTIVE_LOCALITY
            break;

        case TPM_STS_REG:
            value = 0x00000080; // TPM_STS_COMMAND_READY
            if (state.response_size > 0) {
                value |= 0x00000040; // TPM_STS_DATA_AVAIL
            }
            break;

        case TPM_CRB_CTRL_REG:
            value = 0x00000001; // TPM_CRB_CTRL_REQ_SET
            break;

        case TPM_DATA_FIFO_REG:
            if (state.response_size > 0) {
                // Copy response data
                memcpy(&value, state.response_buffer, min(size, 4));

                // Update response buffer
                if (state.response_size > size) {
                    memmove(state.response_buffer,
                        state.response_buffer + size,
                        state.response_size - size);
                    state.response_size -= size;
                }
                else {
                    state.response_size = 0;
                }
            }
            break;

        default:
            return false;
    }

    vcpu->guest_vmcb.state_save_area.rax = value;
    return true;
}

bool tpm_handler::handle_tpm_write(svm::pvcpu_t vcpu, uint64_t offset, uint32_t size, uint64_t value) {
    switch (offset) {
        case TPM_ACCESS_REG:
            state.locality = static_cast<uint8_t>(value & 0x7);
            break;

        case TPM_STS_REG:
            if (value & 0x00000040) { // TPM_STS_COMMAND_READY
                state.is_ready = true;
                state.command_size = 0;
                state.response_size = 0;
            }
            break;

        case TPM_DATA_FIFO_REG:
            if (state.command_size + size <= sizeof(state.command_buffer)) {
                memcpy(state.command_buffer + state.command_size, &value, size);
                state.command_size += size;

                // Process completed command
                if (state.command_size >= sizeof(TPM2_COMMAND_HEADER)) {
                    TPM2_COMMAND_HEADER* header = (TPM2_COMMAND_HEADER*)state.command_buffer;
                    uint32_t commandCode = _byteswap_ulong(header->commandCode);
                    uint32_t paramSize = _byteswap_ulong(header->paramSize);

                    if (state.command_size >= paramSize) {
                        switch (commandCode) {
                            case TPM_CC_ReadPublic: {
                                TPM2_READ_PUBLIC_COMMAND* cmd = (TPM2_READ_PUBLIC_COMMAND*)state.command_buffer;
                                uint32_t handle = _byteswap_ulong(cmd->ObjectHandle);

                                if (handle == TPM20_OBJECT_HANDLE_EK) {
                                    // Construct fake EK response
                                    TPM2_READ_PUBLIC_RESPONSE* response = (TPM2_READ_PUBLIC_RESPONSE*)state.response_buffer;
                                    response->Header.tag = _byteswap_ushort(TPM_ST_NO_SESSIONS);
                                    response->Header.responseCode = _byteswap_ulong(TPM_RC_SUCCESS);

                                    // Set up the public area for RSA 2048 EK
                                    response->OutPublic.size = sizeof(TPMT_PUBLIC);
                                    response->OutPublic.publicArea.type = TPM_ALG_RSA;
                                    response->OutPublic.publicArea.nameAlg = TPM_ALG_SHA256;
                                    response->OutPublic.publicArea.objectAttributes = TPMA_OBJECT_FIXEDTPM |
                                        TPMA_OBJECT_FIXEDPARENT |
                                        TPMA_OBJECT_SENSITIVEDATAORIGIN |
                                        TPMA_OBJECT_ADMINWITHPOLICY |
                                        TPMA_OBJECT_RESTRICTED |
                                        TPMA_OBJECT_DECRYPT;

                                    // Use our custom generated EK key material
                                    memcpy(response->OutPublic.publicArea.unique.rsa,
                                        state.ek_buffer,
                                        sizeof(state.ek_buffer));

                                    state.response_size = sizeof(TPM2_READ_PUBLIC_RESPONSE);
                                }
                                break;
                            }
                        }

                        // Reset command buffer
                        state.command_size = 0;
                        state.is_ready = false;
                    }
                }
            }
            break;

        default:
            return false;
    }

    return true;
}