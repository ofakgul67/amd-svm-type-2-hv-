#pragma once
#include "../includes.h"
#include "../../handlers_ctx/npf/npf_svm.hpp"

// TPM MMIO registers and constants
#define TPM_LOCALITY_0_BASE           0xFED40000
#define TPM_LOCALITY_SIZE             0x1000
#define TPM_ACCESS_REG               0x00
#define TPM_STS_REG                  0x18
#define TPM_DATA_FIFO_REG           0x24
#define TPM_CRB_CTRL_REG            0x40
#define TPM_CRB_CTRL_REQ_REG        0x44
#define TPM_CRB_DATA_BUFFER_REG     0x80

// TPM Command Codes
#define TPM_ST_NO_SESSIONS           0x8001
#define TPM_CC_ReadPublic            0x00000173
#define TPM_RC_SUCCESS               0x00000000
#define TPM_ALG_RSA                  0x0001
#define TPM_ALG_SHA256              0x000B

// TPM Object Attributes
#define TPMA_OBJECT_FIXEDTPM            (1 << 1)
#define TPMA_OBJECT_FIXEDPARENT         (1 << 4)
#define TPMA_OBJECT_SENSITIVEDATAORIGIN (1 << 5)
#define TPMA_OBJECT_ADMINWITHPOLICY     (1 << 7)
#define TPMA_OBJECT_RESTRICTED          (1 << 16)
#define TPMA_OBJECT_DECRYPT             (1 << 17)

// TPM Handle Ranges
#define HR_SHIFT               24
#define HR_PCR                (0x00 << HR_SHIFT)  // PCR (0x00000000)
#define HR_HMAC               (0x02 << HR_SHIFT)  // HMAC key (0x02000000)
#define HR_LOADED_SESSION     (0x02 << HR_SHIFT)  // Loaded Authorization Session (0x02000000)
#define HR_PERSISTENT         (0x81 << HR_SHIFT)  // Persistent Objects (0x81000000)
#define HR_NV_INDEX          (0x01 << HR_SHIFT)  // Non-volatile Index (0x01000000)
#define HR_TRANSIENT         (0x80 << HR_SHIFT)  // Transient Objects (0x80000000)

// Standard TPM Handle Values
#define TPM20_OBJECT_HANDLE_EK        (HR_PERSISTENT | 0x10001)  // 0x81010001
#define TPM20_OBJECT_HANDLE_SRK       (HR_PERSISTENT | 0x10002)  // 0x81010002
#define TPM20_RH_NULL                 0x40000007
#define TPM20_RH_UNASSIGNED          0x40000007
#define TPM20_RS_PW                   0x40000009
#define TPM20_RH_OWNER               0x40000001
#define TPM20_RH_ENDORSEMENT         0x4000000B
#define TPM20_RH_PLATFORM            0x4000000C
#define TPM20_RH_PLATFORM_NV         0x4000000D
#define TPM20_RH_LOCKOUT             0x4000000A

// TPM Command/Response Headers
#pragma pack(push, 1)
struct TPM2_COMMAND_HEADER {
    uint16_t tag;
    uint32_t paramSize;
    uint32_t commandCode;
};

struct TPM2_RESPONSE_HEADER {
    uint16_t tag;
    uint32_t paramSize;
    uint32_t responseCode;
};

// TPM Public Area Structures
struct TPMU_PUBLIC_ID {
    union {
        uint8_t rsa[256];    // For RSA 2048-bit keys
        uint8_t ecc[64];     // For ECC keys
        uint8_t sym[32];     // For symmetric keys
        uint8_t keyedHash[32]; // For keyed hash
    };
};

struct TPMT_PUBLIC {
    uint16_t type;           // Algorithm type (RSA, ECC, etc)
    uint16_t nameAlg;        // Name hashing algorithm
    uint32_t objectAttributes; // Object attributes
    uint8_t authPolicy[32];   // Authorization policy
    uint16_t parameters[12];  // Key parameters (size depends on type)
    TPMU_PUBLIC_ID unique;    // The public key material
};

struct TPM2B_PUBLIC {
    uint16_t size;
    TPMT_PUBLIC publicArea;
};

// Additional helper structures
struct TPM2B_NAME {
    uint16_t size;
    uint8_t name[sizeof(uint16_t) + 32]; // Algorithm ID + Digest
};

// Command Specific Structures
struct TPM2_READ_PUBLIC_COMMAND {
    TPM2_COMMAND_HEADER Header;
    uint32_t ObjectHandle;
};

struct TPM2_READ_PUBLIC_RESPONSE {
    TPM2_RESPONSE_HEADER Header;
    TPM2B_PUBLIC OutPublic;
    TPM2B_NAME OutName;
    TPM2B_NAME OutQualifiedName;
};

struct TPM2B_DATA {
    uint16_t size;
    uint8_t buffer[sizeof(TPMT_PUBLIC)];
};

class tpm_handler {
public:
    static tpm_handler* get();
    bool initialize(svm::pvcpu_t vcpu);
    void cleanup(svm::pvcpu_t vcpu);

    // Handler for NPF exit when accessing TPM MMIO
    bool handle_tpm_mmio(svm::pvcpu_t vcpu, const page_context& ctx);

    // Setup NPT hooks for TPM MMIO range
    bool setup_tpm_hooks(svm::pvcpu_t vcpu);

    // Remove NPT hooks for TPM MMIO range
    bool remove_tpm_hooks(svm::pvcpu_t vcpu);

private:
    tpm_handler() = default;
    ~tpm_handler() = default;

    // TPM state tracking
    struct tpm_state {
        uint8_t locality;
        bool is_ready;
        uint32_t command_size;
        uint8_t command_buffer[4096];
        uint32_t response_size;
        uint8_t response_buffer[4096];
        uint8_t ek_buffer[256]; // Fake endorsement key
    } state;

    // Internal handlers
    bool handle_tpm_read(svm::pvcpu_t vcpu, uint64_t offset, uint32_t size);
    bool handle_tpm_write(svm::pvcpu_t vcpu, uint64_t offset, uint32_t size, uint64_t value);
    void generate_fake_ek();

    // Utility functions
    bool is_tpm_address(uint64_t pa) const {
        return (pa >= TPM_LOCALITY_0_BASE &&
            pa < (TPM_LOCALITY_0_BASE + TPM_LOCALITY_SIZE));
    }

    // Synchronization
    KSPIN_LOCK tpm_lock;
    void acquire_lock(PKIRQL old_irql) {
        KeAcquireSpinLock(&tpm_lock, old_irql);
    }
    void release_lock(KIRQL old_irql) {
        KeReleaseSpinLock(&tpm_lock, old_irql);
    }
};

