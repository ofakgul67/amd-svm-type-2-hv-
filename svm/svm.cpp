#include "svm.hpp"

extern "C" void __stdcall vmenter(PVOID host_rsp);
extern "C" u64 __stdcall svm_vmmcall(vmmcall_codes vmmcall_id, ...);

void hypervisor::initialize() {
	for (int i = 0; i < 32; ++i) {
		vcpus[i] = nullptr;
	}
}

hypervisor* hypervisor::get() {
	static hypervisor instance;
	static bool is_initialized = false;

	if (!is_initialized) {
		instance.initialize();
		is_initialized = true;
	}
	return &instance;
}

bool hypervisor::is_core_virtualized(u8 core_number) {
	return get()->vcpus[core_number] != NULL ? true : false;
}

void hypervisor::cleanup_on_exit() {
	hypervisor* instance = get();

	// Clean up VCPU resources for each core
	for (int i = 0; i < 32; ++i) {
		if (instance->vcpus[i]) {
			memory::get()->free_nonpaged_pool(instance->vcpus[i]);
			instance->vcpus[i] = nullptr;
		}
	}
}

void virtualization::setup_control_fields(svm::pvcpu_t vcpu) {
	auto& control = vcpu->guest_vmcb.control_area;

	// Set debug register intercepts - intercept all debug registers 0-15
	control.intercept_dr_read = 0xFFFF;   // Intercept DR0-DR15 reads
	control.intercept_dr_write = 0xFFFF;  // Intercept DR0-DR15 writes

	// Intercept MISC1
	control.intercept_misc1 |= misc1_intercept::intercept_cpuid;
	control.intercept_misc1 |= misc1_intercept::intercept_msr_prot;
	control.intercept_misc1 |= misc1_intercept::intercept_rdtsc;
	control.intercept_misc1 |= misc1_intercept::intercept_invlpga;

	// Intercept MISC2
	control.intercept_misc2 |= misc2_intercept::intercept_vmcall;
	control.intercept_misc2 |= misc2_intercept::intercept_vmrun;
	control.intercept_misc2 |= misc2_intercept::intercept_vmload;
	control.intercept_misc2 |= misc2_intercept::intercept_vmsave;
	control.intercept_misc2 |= misc2_intercept::intercept_clgi;
	control.intercept_misc2 |= misc2_intercept::intercept_stgi;
	control.intercept_misc2 |= misc2_intercept::intercept_skinit;
	control.intercept_misc2 |= misc2_intercept::intercept_xsetbv;
	control.intercept_misc2 |= misc2_intercept::intercept_cr3;
	control.intercept_misc2 |= misc2_intercept::intercept_cr4;

	// Setup exception intercepts
	control.intercept_exception |= exception_intercept::intercept_ud;

	// Guest Address Space ID (ASID)
	control.guest_asid = 1;

	// Setup MSRPM
	control.msrpm_base_pa = MmGetPhysicalAddress(&vcpu->msrpm).QuadPart;

	// Setup LBR
	control.lbr_virtualization_enable = 0;

	// Setup NPT
	control.np_enable = 1;
	control.ncr3 = MmGetPhysicalAddress(&vcpu->npt[npt::nested_page_table::primary]).QuadPart;
}

void virtualization::setup_guest(svm::pvcpu_t vcpu, const svm::pcontext_t ctx) {
	// Global variables
	auto& state = vcpu->guest_vmcb.state_save_area;
	auto& host_stack = vcpu->host_stack_layout;

	// Segment registers
	auto& cs = state.cs;
	auto& ds = state.ds;
	auto& es = state.es;
	auto& ss = state.ss;

	// Load initial guest state based on current state
	svm::descriptor_table_register_t idtr{}, gdtr{}; __sidt(&idtr); _sgdt(&gdtr);
	state.gdtr.base = gdtr.base;
	state.gdtr.limit = gdtr.limit;
	state.idtr.base = idtr.base;
	state.idtr.limit = idtr.limit;

	// Setup control registers
	state.cr0.value = __readcr0();
	state.cr2.value = __readcr2();
	state.cr3.value = __readcr3();
	state.cr4.value = __readcr4();
	state.efer.value = __readmsr(EFER);
	state.gpat = __readmsr(PAT);
	state.lstar = __readmsr(LSTAR);
	state.dbg_ctl = __readmsr(DEBUG_CTL);
	state.cstar = __readmsr(CSTAR);
	state.sfmask = __readmsr(SFMASK);
	state.sysenter_cs = __readmsr(SYSENTER_CS);
	state.sysenter_esp = __readmsr(SYSENTER_ESP);
	state.sysenter_eip = __readmsr(SYSENTER_EIP);

	// Setup important registers
	state.rsp = ctx->rsp;
	state.rip = ctx->rip;
	state.rax = ctx->rax;
	state.rflags = __readeflags();

	// Setup all the segment registers
	cs.selector.value = __read_cs();
	cs.limit = __segmentlimit(cs.selector.value);
	cs.get_attributes(gdtr.base);

	ds.selector.value = __read_ds();
	ds.limit = __segmentlimit(ds.selector.value);
	ds.get_attributes(gdtr.base);

	es.selector.value = __read_es();
	es.limit = __segmentlimit(es.selector.value);
	es.get_attributes(gdtr.base);

	ss.selector.value = __read_ss();
	ss.limit = __segmentlimit(ss.selector.value);
	ss.get_attributes(gdtr.base);

	// Pass initial values to store as shadow copies
	host_stack.shadow_data.efer.value = vcpu->guest_vmcb.state_save_area.efer.value;
	host_stack.shadow_data.efer.svme = 0; // treated as disabled until the guest enables it
	host_stack.shadow_data.hsave_pa.value = 0;

	// Stack setup for vmlaunch
	host_stack.reserved1 = MAXUINT64;
	host_stack.shared_vcpu = nullptr;
	host_stack.self = vcpu;
	host_stack.host_vmcb_pa = MmGetPhysicalAddress(&vcpu->host_vmcb).QuadPart;
	host_stack.guest_vmcb_pa = MmGetPhysicalAddress(&vcpu->guest_vmcb).QuadPart;

	// These are restored to the processor right before vmexit using vmload, 
	// so that the guest can continue it's execution with the saved state
	__svm_vmsave(host_stack.host_vmcb_pa);
	__svm_vmsave(host_stack.guest_vmcb_pa);
}

void virtualization::setup_host(svm::pvcpu_t vcpu) {
	// Set up mapping host CR3
	svm::cr3_t& host_cr3 = vcpu->host_vmcb.state_save_area.cr3;
	host_cr3.value = 0;
	host_cr3.pcd = 0;
	host_cr3.pwt = 0;
	host_cr3.pml4 =
		MmGetPhysicalAddress(&hypervisor::get()->shared_host_pt.pml4).QuadPart >> 12;

	svm::cr4_t& host_cr4 = vcpu->host_vmcb.state_save_area.cr4;
	host_cr4.value = __readcr4();

	// These are flags that may or may not be set by Windows
	host_cr4.fsgsbase = 1;
	host_cr4.osxsave = 1;
	host_cr4.smap = 0;
	host_cr4.smep = 0;

	// Load host CRs.
	__writecr3(host_cr3.value);
	__writecr4(host_cr4.value);

	// Set an address of the host state area to VM_HSAVE:PA MSR. 
	// the processor saves some of the current state on vmrun and loads them on vmexit
	svm::hsave_pa_msr hsave_pa{ .value = static_cast<uint64_t>(MmGetPhysicalAddress(&vcpu->host_state_area).QuadPart) };

	// Set the pat entries for different memory types.
	svm::pat_msr host_pat{};
	host_pat.pa0 = svm::attribute_type::write_back;
	host_pat.pa1 = svm::attribute_type::write_through;
	host_pat.pa2 = svm::attribute_type::uncacheable_no_write_combinining;
	host_pat.pa3 = svm::attribute_type::uncacheable;
	host_pat.pa4 = svm::attribute_type::write_back;
	host_pat.pa5 = svm::attribute_type::write_through;
	host_pat.pa6 = svm::attribute_type::uncacheable_no_write_combinining;
	host_pat.pa7 = svm::attribute_type::uncacheable;

	// write the address of the host state area to VM_HSAVE:PA MSR.
	__writemsr(VM_HSAVE_PA, hsave_pa.value);

	// write the pat entries to the msr for the pat.
	__writemsr(PAT, host_pat.value);
}

bool virtualization::virtualize_processor(PVOID ctx) {
	svm::pvcpu_t* vcpu = hypervisor::get()->vcpus;
	svm::context_t context{};
	svm::processor_number processor_number{};
	svm::pcontext_t current_ctx =
		reinterpret_cast<svm::pcontext_t>(context.get_current_context());
	if (!current_ctx) {
		return false;
	}

	u8 processor_index = processor_number.get_processor_index();
	if (hypervisor::get()->is_core_virtualized(processor_index) == false) {
		vcpu[processor_index] = reinterpret_cast<svm::pvcpu_t>(
			memory::get()->alloc_nonpaged_pool(sizeof(svm::vprocessor_data)));
		if (!vcpu[processor_index]) {
			debug::panicf("Failed allocating memory for VCPU\n");
			return false;
		}

		// Prepare the Nested Page Table (NPT) - Primary
		if (npt::get()->prepare_npt(vcpu[processor_index]->npt[npt::nested_page_table::primary]) != svm::svmroot_error_t::error_success) {
			return false;
		}

		// Prepare the Nested Page Table (NPT) - Shadow
		if (npt::get()->prepare_npt(vcpu[processor_index]->npt[npt::nested_page_table::shadow]) != svm::svmroot_error_t::error_success) {
			return false;
		}

		// Prepare the MSR permissions bitmap to control access to certain MSRs
		if (msrs::get()->prepare_msrpm(vcpu[processor_index]->msrpm) != svm::svmroot_error_t::error_success) {
			return false;
		}

		// Enable EFER.SVME
		svm::efer_msr efer{};
		efer.value = __readmsr(msrs::EFER);
		efer.svme = 1;
		__writemsr(msrs::EFER, efer.value);

		// Setup Control Fields
		virtualization::setup_control_fields(vcpu[processor_index]);

		// Setup Guest VMCB
		virtualization::setup_guest(vcpu[processor_index], current_ctx);

		// Setup Host VMCB
		virtualization::setup_host(vcpu[processor_index]);

		debug::panicf("Processor %u virtualized successfully\n", processor_index);

		// Enter VM
		vmenter(&vcpu[processor_index]->host_stack_layout.guest_vmcb_pa);

		// we should never get to this part
		KeBugCheck(0xB16B00B5UL);
	}

	return true;
}

auto virtualization::devirtualize_processor(PVOID ctx) -> bool {
	// Get the current processor index
	svm::processor_number processor_number{};
	svm::pvcpu_t vcpu = nullptr;
	u8 processor_index = processor_number.get_processor_index();

	// Check if the core is virtualized
	if (hypervisor::get()->is_core_virtualized(processor_index)) {
		// Ask hypervisor to unload
		svm_vmmcall(vmmcall_codes::disable_hv, nullptr);

		// Retrieve the VCPU for the current processor
		vcpu = hypervisor::get()->vcpus[processor_index];
		if (!vcpu) {
			debug::panicf("Failed to retrieve VCPU for devirtualization\n");
			return false;
		}

		// Free the VCPU memory
		memory::get()->free_nonpaged_pool(vcpu);

		debug::panicf("Processor %u is devirtualized\n", processor_index);
	}
	else {
		debug::panicf("Processor %u is not virtualized\n", processor_index);
		return false;
	}

	return true;
}

auto virtualization::virtualize() -> void {
	// Initialize logger
	logs::get()->initialize();

	if (utilities::check_support()) {
		// First prepare host page tables
		if (memory::get()->prepare_host_page_tables() != svm::svmroot_error_t::error_success) {
			debug::panicf("Failed preparing host page tables\n");
			return;
		}

		// Second pass: Complete virtualization
		if (!utilities::execute_on_each_processor(virtualize_processor, nullptr)) {
			debug::panicf("Failed executing on each processor\n");
			return;
		}

		// After NPT is set up, protect critical structures
		critical_protector::get()->protect_critical_structures(hypervisor::get()->vcpus[0]);

		// Initialize TPM handler after NPT and critical structures are set up
		// Use vcpu[0] since TPM is system-wide and we only need to set it up once
		if (!tpm_handler::get()->initialize(hypervisor::get()->vcpus[0])) {
			debug::panicf("Failed initializing TPM handler\n");
			return;
		}
	}
	else {
		debug::panicf("Your processor does not support virtualization\n");
	}
}

auto virtualization::devirtualize() -> void {
	// Clean up TPM handler first
	tpm_handler::get()->cleanup(hypervisor::get()->vcpus[0]);

	// Clean up nested page tables (NPT)
	npt::get()->cleanup();

	// Devirtualize each processor
	if (!utilities::execute_on_each_processor(devirtualize_processor, nullptr)) {
		debug::panicf("Failed devirtualizing on each processor\n");
	}
}
