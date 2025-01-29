#pragma once
#include "../includes.h"

enum vmmcall_codes : u64 {
    // System Management (0x10000000 - 0x10000FFF)
    disable_hv = 0x10000000,

    // Memory Operations (0x10001000 - 0x10001FFF)
    read_physical_memory = 0x10001000,
    write_physical_memory = 0x10001001,
    read_virtual_memory = 0x10001002,
    write_virtual_memory = 0x10001003,
    get_physical_address = 0x10001004,
    get_virtual_address = 0x10001005,

    // Process Operations (0x10002000 - 0x10002FFF)
    get_process_cr3 = 0x10002000,
    get_process_base = 0x10002001,
    get_process_module = 0x10002002,

    // Hook Management (0x10003000 - 0x10003FFF)
    install_function_hook = 0x10003000,
    remove_function_hook = 0x10003001,
    install_npt_hook = 0x10003002,
    remove_npt_hook = 0x10003003,

    // TPM Management (0x10004000 - 0x10004FFF)
    install_tpm_spoofer = 0x10004000,
    remove_tpm_spoofer = 0x10004001,

    // Page Hiding Operations (0x10005000 - 0x10005FFF)
    hide_physical_page = 0x10005000,
    unhide_physical_page = 0x10005001,

    // Hypervisor Information (0x10006000 - 0x10006FFF)
    get_hv_base = 0x10006000,

    // Logger Operations (0x10007000 - 0x10007FFF)
    flush_logs = 0x10007000,
    write_log = 0x10007001,
};

class vmmcall_params {
public:
    struct get_hv_base_params {
        u64 hv_base;
        u64 hv_size;
    };

    struct install_npt_hook_params {
        u64 original_page_pfn;
        u64 executable_page_pfn;
        u64 result;
    };

    struct remove_npt_hook_params {
        u64 original_page_pfn;
        u64 result;
    };

    struct install_function_hook_params {
        void* address;
        void* proxy;
        void** detour;
        u64 result;
    };

    struct remove_function_hook_params {
        void* address;     
        u64 result;        
    };

    struct read_virtual_memory_params {
        svm::cr3_t process_cr3;
        u64 destination_gva;
        u64 source_gva;
        u64 size;
        u64 result;
    };

    struct write_virtual_memory_params {
        svm::cr3_t process_cr3;
        u64 destination_gva;
        u64 source_gva;
        u64 size;
        u64 result;
    };

    struct read_physical_memory_params {
        u64 destination_gva;
        u64 source_gva;
        u64 size;
        u64 result;
    };

    struct write_physical_memory_params {
        u64 destination_gva;
        u64 source_gva;
        u64 size;
        u64 result;
    };

    struct get_physical_address_params {
        svm::cr3_t process_cr3;
        u64 virtual_address;
        u64 physical_address;
    };

    struct get_virtual_address_params {
        svm::cr3_t process_cr3;
        u64 physical_address;
        u64 virtual_address;    
    };

    struct get_process_cr3_params {
        u64 target_pid;
        u64 process_cr3;
    };

    struct get_process_base_params {
        u64 target_pid;
        u64 process_base;
    };

    struct get_process_module_params {
        u64 target_pid;
        const wchar_t* module_name;
        u64 process_module;
    };

    struct hide_physical_page_params {
        u64 page_pfn;
        u64 result;
    };

    struct unhide_physical_page_params {
        u64 page_pfn;
        u64 result;
    };

    struct flush_logs_params {
        u64 buffer_gva;   
        u32 count;        
        u64 result;
    };

    struct write_log_params {
        char message[128];  
        u64 result;
    };

    struct install_tpm_spoofer_params {
        u64 result;
    };

    struct remove_tpm_spoofer_params {
        u64 result;
    };
};

class vmmcall_handler {
private:
    u8 get_current_cpu();

    template<typename T>
    T get_command_internal(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);

    template<typename T>
    void set_command_internal(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, const T& params);

public:
    static vmmcall_handler* get();
    void vmmcall_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx);
};
