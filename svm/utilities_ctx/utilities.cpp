#include "utilities.hpp"
#include "../svm.hpp"

bool utilities::is_amd() {
	int registers[4]; // eax ebx ecx edx

	__cpuid(registers, static_cast<int>(svm::cpuid::cpu_vendor_string));

	// an AMD processor should always return AuthenticAMD
	return ((registers[1] == 'htuA') &&
		(registers[3] == 'itne') &&
		(registers[2] == 'DMAc'));
}

bool utilities::is_svm_supported() {
	int registers[4]; // eax ebx ecx edx

	__cpuid(registers, static_cast<int>(svm::cpuid::svm_features_ex));
	return (registers[2] & static_cast<int>(svm::cpuid::cpu_ecx_svm));
}

bool utilities::is_npt_supported() {
	int registers[4]; // eax ebx ecx edx

	__cpuid(registers, static_cast<int>(svm::cpuid::svm_features));
	return registers[3] & static_cast<int>(svm::cpuid::cpu_edx_np);
}

bool utilities::can_svm_be_enabled() {
	uint64_t vmcr;
	vmcr = __readmsr(static_cast<int>(svm::msr::svm_vm_cr));

	// if VMCR.SVMDIS is enabled SVM cannot be enabled
	// "The VMRUN, VMLOAD, VMSAVE, CLGI, VMMCALL, and INVLPGA instructions can be used
	// when the EFER.SVME is set to 1; otherwise, these instructions generate a #UD exception"
	return !(vmcr & static_cast<int>(svm::msr::svm_vm_cr_svmdis));
}

bool utilities::is_svm_disabled_at_boot() {
	int registers[4]; // eax ebx ecx edx

	__cpuid(registers, static_cast<int>(svm::cpuid::svm_features));
	return (registers[3] & static_cast<int>(svm::cpuid::cpu_edx_np));
}

bool utilities::is_hypervisor_vendor_installed() {
    int registers[4];   // eax ebx ecx edx
    char vedor_id[13];

    // when hypervisor is active, CPUID leaf 0x40000000
    // will return our own hypervisor vendor
    __cpuid(registers, static_cast<int>(svm::cpuid::hypervisor_vendor_id));
    RtlCopyMemory(vedor_id + 0, &registers[1], sizeof(registers[1]));
    RtlCopyMemory(vedor_id + 4, &registers[2], sizeof(registers[2]));
    RtlCopyMemory(vedor_id + 8, &registers[3], sizeof(registers[3]));
    vedor_id[12] = ANSI_NULL;

    return (strcmp(vedor_id, "svm         ") == 0);
}

bool utilities::check_support() {
	return is_amd() &&
		is_svm_supported() &&
		is_npt_supported() &&
		can_svm_be_enabled() &&
		is_svm_disabled_at_boot() &&
        !is_hypervisor_vendor_installed();
}

void* utilities::page_frame_number_to_virtual_address(u64 pfn) {
	PHYSICAL_ADDRESS pa;
	pa.QuadPart = pfn << PAGE_SHIFT;
	return MmGetVirtualForPhysical(pa);
}

ULONG utilities::get_patch_size(PVOID code, ULONG min_hook_length) {
    if (!code || !min_hook_length) {
        return 0;
    }

    ULONG total_size = 0;

    // Keep accumulating instruction lengths until we meet or exceed the minimum hook length
    while (total_size < min_hook_length) {
        PVOID current_instruction = reinterpret_cast<PVOID>(
            reinterpret_cast<ULONG_PTR>(code) + total_size
            );

#if defined(_WIN64)
        const uint8_t arch_bits = 64;
#else
        const uint8_t arch_bits = 32;
#endif

        // Get length of current instruction using our disassembler
        uint32_t instruction_length = disassembler::get()->decode_length(
            current_instruction,
            arch_bits
        );

        // Safety check for invalid instructions
        if (instruction_length == 0) {
            return 0;  // Return 0 to indicate error
        }

        total_size += instruction_length;
    }

    return total_size;
}

int utilities::exponent(int base, int power) {
    int start = 1;
    for (int i = 0; i < power; ++i){
        start *= base;
    }

    return start;
}

void utilities::enable_svme() {
    svm::efer_msr	msr;
    msr.value = __readmsr(svm::efer);
    msr.svme = 1;
    __writemsr(svm::efer, msr.value);
}

PMDL utilities::lock_pages(void* virtual_address, LOCK_OPERATION operation, int size) {
    PMDL mdl = IoAllocateMdl(virtual_address, size, FALSE, FALSE, nullptr);
    MmProbeAndLockPages(mdl, KernelMode, operation);
    return mdl;
}

NTSTATUS utilities::unlock_pages(PMDL mdl) {
    MmUnlockPages(mdl);
    IoFreeMdl(mdl);
    return STATUS_SUCCESS;
}

auto utilities::copy_memory_safe(void* dest, void* src, unsigned long long length) -> bool {
    PMDL mdl = IoAllocateMdl(dest, length, FALSE, FALSE, NULL);
    if (!mdl) {
        return false;
    }

    MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);

    PVOID mapped = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, 0, HighPagePriority);
    if (!mapped) {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        return false;
    }

    memcpy(mapped, src, length);

    MmUnmapLockedPages(mapped, mdl);
    MmUnlockPages(mdl);
    IoFreeMdl(mdl);
    return true;
}

bool utilities::execute_on_each_processor(callback callback, PVOID ctx) {
    // we need to be running at an IRQL below DISPATCH_LEVEL
    NT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);

    // get total number of processors
    unsigned long processor_count = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);

    // execute on each processor
    for (unsigned long i = 0; i < processor_count; ++i) {
        // restrict execution to the specified cpu
        auto const orig_affinity = KeSetSystemAffinityThreadEx(1ull << i);

        if (!callback(ctx)) {
            // revert affinity if callback fails
            KeRevertToUserAffinityThreadEx(orig_affinity);
            return false;
        }

        // revert affinity after successful execution
        KeRevertToUserAffinityThreadEx(orig_affinity);
    }

    return true;
}

PIMAGE_SECTION_HEADER utilities::get_section(void* image_base, const char* section_name) {
    PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(image_base);
    PIMAGE_NT_HEADERS32 nt_header = reinterpret_cast<PIMAGE_NT_HEADERS32>(dos_header->e_lfanew + reinterpret_cast<ULONG64>(image_base));
    ULONG num_sections = nt_header->FileHeader.NumberOfSections;
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_header);

    STRING target_section_name;
    RtlInitString(&target_section_name, section_name);

    for (ULONG i = 0; i < num_sections; i++) {
        STRING current_section_name;
        RtlInitString(&current_section_name, reinterpret_cast<PCSZ>(section->Name));
        if (current_section_name.Length > 8)
            current_section_name.Length = 8;

        if (RtlCompareString(&current_section_name, &target_section_name, FALSE) == 0) {
            return section;
        }
        section++;
    }

    return nullptr;
}

unsigned long long utilities::get_exported_function(const unsigned long long module_address, const char* proc_name) {
    const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_address);
    const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<ULONGLONG>(dos_header) + dos_header->e_lfanew);

    const auto data_directory = nt_headers->OptionalHeader.DataDirectory[0];
    const auto export_directory = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(module_address + data_directory.VirtualAddress);

    const auto address_of_names = reinterpret_cast<ULONG*>(module_address + export_directory->AddressOfNames);

    for (size_t i = 0; i < export_directory->NumberOfNames; i++) {
        const auto function_name = reinterpret_cast<const char*>(module_address + address_of_names[i]);

        if (!_stricmp(function_name, proc_name)) {
            const auto name_ordinal = reinterpret_cast<unsigned short*>(module_address + export_directory->AddressOfNameOrdinals)[i];

            const auto function_rva = module_address + reinterpret_cast<ULONG*>(module_address + export_directory->AddressOfFunctions)[name_ordinal];
            return function_rva;
        }
    }

    return 0;
}

void utilities::get_windows_version() {
    RTL_OSVERSIONINFOW ver = { 0 };
    RtlGetVersion(&ver);
    windows_build_number = ver.dwBuildNumber;
}

void* utilities::get_process_id_by_name(const wchar_t* process_name) {
    CHAR image_name[15];
    PEPROCESS sys_process = PsInitialSystemProcess;
    PEPROCESS cur_entry = sys_process;

    do {
        RtlCopyMemory((PVOID)(&image_name), (PVOID)((uintptr_t)cur_entry + image_file_name), sizeof(image_name));

        if (crt_strcmp(image_name, process_name, true)) {
            u32 active_threads_value;
            RtlCopyMemory((PVOID)&active_threads_value, (PVOID)((uintptr_t)cur_entry + active_threads), sizeof(active_threads_value));

            if (active_threads_value)
                return PsGetProcessId(cur_entry);
        }

        PLIST_ENTRY list = (PLIST_ENTRY)((uintptr_t)(cur_entry)+active_process_links);
        cur_entry = (PEPROCESS)((uintptr_t)list->Flink - active_process_links);

    } while (cur_entry != sys_process);

    return 0;
}