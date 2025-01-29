#include "vmmcall_svm.hpp"

vmmcall_handler* vmmcall_handler::get() {
    static vmmcall_handler instance;
    static bool is_initialized = false;

    if (!is_initialized) {
        is_initialized = true;
    }
    return &instance;
}

template<typename T>
T vmmcall_handler::get_command_internal(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    auto cr3 = page_table::get()->switch_to_guest_cr3(vcpu);
    T* params = reinterpret_cast<T*>(guest_ctx->vprocessor_registers->rdx.value);
    T local_params = *params;
    page_table::get()->restore_cr3(cr3);
    return local_params;
}

template<typename T>
void vmmcall_handler::set_command_internal(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx, const T& params) {
    auto cr3 = page_table::get()->switch_to_guest_cr3(vcpu);
    T* guest_params = reinterpret_cast<T*>(guest_ctx->vprocessor_registers->rdx.value);
    *guest_params = params;
    page_table::get()->restore_cr3(cr3);
}

void vmmcall_handler::vmmcall_exit_handler(svm::pvcpu_t vcpu, svm::pguest_ctx_t guest_ctx) {
    u64 vmmcall_func = guest_ctx->vprocessor_registers->rcx.value;
    u8 cpu_index = get_current_cpu();

    switch (vmmcall_func) {
    case vmmcall_codes::disable_hv: {
        guest_ctx->should_exit = true;
        break;
    }
    case vmmcall_codes::get_hv_base: {
        auto params = get_command_internal<vmmcall_params::get_hv_base_params>(vcpu, guest_ctx);
        params.hv_base = reinterpret_cast<u64>(&__ImageBase);
        params.hv_size = (reinterpret_cast<PIMAGE_NT_HEADERS>(
            reinterpret_cast<u64>(&__ImageBase) +
            reinterpret_cast<PIMAGE_DOS_HEADER>(&__ImageBase)->e_lfanew
            ))->OptionalHeader.SizeOfImage;
        set_command_internal<vmmcall_params::get_hv_base_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::get_process_cr3: {
        auto params = get_command_internal<vmmcall_params::get_process_cr3_params>(vcpu, guest_ctx);
        params.process_cr3 = memory::get()->get_process_cr3(params.target_pid);
        set_command_internal<vmmcall_params::get_process_cr3_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::get_process_base: {
        auto params = get_command_internal<vmmcall_params::get_process_base_params>(vcpu, guest_ctx);
        params.process_base = memory::get()->get_process_base(params.target_pid);
        set_command_internal<vmmcall_params::get_process_base_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::get_process_module: {
        auto params = get_command_internal<vmmcall_params::get_process_module_params>(vcpu, guest_ctx);
        params.process_module = memory::get()->get_process_module(
            vcpu,
            params.target_pid,
            params.module_name
        );
        set_command_internal<vmmcall_params::get_process_module_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::get_physical_address: {
        auto params = get_command_internal<vmmcall_params::get_physical_address_params>(vcpu, guest_ctx);
        params.physical_address = page_table::get()->get_physical_address(
            vcpu,
            params.process_cr3,
            params.virtual_address
        );
        set_command_internal<vmmcall_params::get_physical_address_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::get_virtual_address: {
        auto params = get_command_internal<vmmcall_params::get_virtual_address_params>(vcpu, guest_ctx);
        params.virtual_address = page_table::get()->get_virtual_address(
            vcpu,
            params.process_cr3,
            params.physical_address
        );
        set_command_internal<vmmcall_params::get_virtual_address_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::write_virtual_memory: {
        auto params = get_command_internal<vmmcall_params::write_virtual_memory_params>(vcpu, guest_ctx);
        params.result = memory_access::get()->write_virtual_memory(
            vcpu,
            params.process_cr3,
            params.destination_gva,
            params.source_gva,
            params.size
        );
        set_command_internal<vmmcall_params::write_virtual_memory_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::read_virtual_memory: {
        auto params = get_command_internal<vmmcall_params::read_virtual_memory_params>(vcpu, guest_ctx);
        params.result = memory_access::get()->read_virtual_memory(
            vcpu,
            params.process_cr3,
            params.destination_gva,
            params.source_gva,
            params.size
        );
        set_command_internal<vmmcall_params::read_virtual_memory_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::write_physical_memory: {
        auto params = get_command_internal<vmmcall_params::write_physical_memory_params>(vcpu, guest_ctx);
        params.result = memory_access::get()->write_physical_memory(
            vcpu,
            params.destination_gva,
            params.source_gva,
            params.size
        );
        set_command_internal<vmmcall_params::write_physical_memory_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::read_physical_memory: {
        auto params = get_command_internal<vmmcall_params::read_physical_memory_params>(vcpu, guest_ctx);
        params.result = memory_access::get()->read_physical_memory(
            vcpu,
            params.destination_gva,
            params.source_gva,
            params.size
        );
        set_command_internal<vmmcall_params::read_physical_memory_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::install_function_hook: {
        auto params = get_command_internal<vmmcall_params::install_function_hook_params>(vcpu, guest_ctx);
        params.result = hook::get()->install_function_hook(
            vcpu,
            params.address,
            params.proxy,
            params.detour
        );
        set_command_internal<vmmcall_params::install_function_hook_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::remove_function_hook: {
        auto params = get_command_internal<vmmcall_params::remove_function_hook_params>(vcpu, guest_ctx);
        params.result = hook::get()->remove_function_hook(
            vcpu,
            params.address
        );
        set_command_internal<vmmcall_params::remove_function_hook_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::install_npt_hook: {
        auto params = get_command_internal<vmmcall_params::install_npt_hook_params>(vcpu, guest_ctx);
        params.result = npt::get()->install_npt_hook(
            vcpu,
            vcpu->npt[npt::nested_page_table::primary],
            vcpu->npt[npt::nested_page_table::shadow],
            params.original_page_pfn,
            params.executable_page_pfn
        );
        set_command_internal<vmmcall_params::install_npt_hook_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::remove_npt_hook: {
        auto params = get_command_internal<vmmcall_params::remove_npt_hook_params>(vcpu, guest_ctx);
        params.result = npt::get()->remove_npt_hook(
            vcpu,
            vcpu->npt[npt::nested_page_table::shadow],
            params.original_page_pfn
        );
        set_command_internal<vmmcall_params::remove_npt_hook_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::install_tpm_spoofer: {
        auto params = get_command_internal<vmmcall_params::install_tpm_spoofer_params>(vcpu, guest_ctx);
        //params.result = tpm_spoofer::get()->install_tpm_spoofer(
        //    vcpu
        //);
        set_command_internal<vmmcall_params::install_tpm_spoofer_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::remove_tpm_spoofer: {
        auto params = get_command_internal<vmmcall_params::remove_tpm_spoofer_params>(vcpu, guest_ctx);
        //params.result = tpm_spoofer::get()->remove_tpm_spoofer(
        //    vcpu
        //);
        set_command_internal<vmmcall_params::remove_tpm_spoofer_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::hide_physical_page: {
        auto params = get_command_internal<vmmcall_params::hide_physical_page_params>(vcpu, guest_ctx);
        params.result = hide_page::get()->hide_physical_page(
            vcpu,
            params.page_pfn
        );
        set_command_internal<vmmcall_params::hide_physical_page_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::unhide_physical_page: {
        auto params = get_command_internal<vmmcall_params::unhide_physical_page_params>(vcpu, guest_ctx);
        params.result = hide_page::get()->unhide_physical_page(
            vcpu,   
            params.page_pfn
        );
        set_command_internal<vmmcall_params::unhide_physical_page_params>(vcpu, guest_ctx, params);
        break;
    }
    case vmmcall_codes::flush_logs: {
        auto params = get_command_internal<vmmcall_params::flush_logs_params>(vcpu, guest_ctx);
        params.result = static_cast<u32>(
            logs::get()->flush_logs(vcpu, params.buffer_gva, params.count)
            );
        set_command_internal(vcpu, guest_ctx, params);
        break;  
    }
    case vmmcall_codes::write_log: {
        auto params = get_command_internal<vmmcall_params::write_log_params>(vcpu, guest_ctx);
        logs::get()->write("%s", params.message);
        params.result = svm::svmroot_error_t::error_success;
        set_command_internal(vcpu, guest_ctx, params);
        break;
    }
    default: {
        event_inj_t{}.invalid_opcode_exception(vcpu);
        return;
    }
    }

    vcpu->guest_vmcb.state_save_area.rip = vcpu->guest_vmcb.control_area.next_rip;
}

u8 vmmcall_handler::get_current_cpu() {
    return static_cast<u8>(KeGetCurrentProcessorNumber());
}