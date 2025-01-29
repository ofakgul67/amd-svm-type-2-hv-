#pragma once
#include <ntifs.h>
#include <classpnp.h>
#include <ntifs.h>
#include <ntddk.h>
#include <windef.h>
#include <ntimage.h>
#pragma warning (disable : 4201) 
#pragma warning (disable : 4083) 
#pragma warning (disable : 4005) 
#pragma warning (disable : 4100)
#pragma warning (disable : 5040)
#pragma warning (disable : 4083)
#include <intrin.h>
#include <stdint.h>

#include "Zydis/Zydis.h"

// Debug logger
#define debug_log(text, ...) (DbgPrintEx(0, 0, "[CryWare] " text, ##__VA_ARGS__))
#define debug_break() __debugbreak();

// Unreference Parameters/Variables
#define unref_var(x)	(x=x)
#define unref_param(x)	(x=x)

// Allocation tag
#define cv_tag 'CryHv'

// SVM
#define svm_exitinfo_reg_mask           0x0F
#define svm_intercept_popf              17
#define svm_intercept_pushf             16
#define svm_np_enable                   0

// Function hook
#define hook_length				23
#define detour_length			14

// PAGE
#define page_4kb 0x1000
#define page_2mb page_4kb * 512
#define page_1gb page_2mb * 512

// Following designation allows us to setup and dump VMCB
// data in the way that we perform on Intel VT-x driver.
#define svm_vmread8(v,o)        *(u8*)((ulong_ptr)v+o)
#define svm_vmread16(v,o)       *(u16*)((ulong_ptr)v+o)
#define svm_vmread32(v,o)       *(u32*)((ulong_ptr)v+o)
#define svm_vmread64(v,o)       *(u64*)((ulong_ptr)v+o)

#define svm_vmwrite8(v,o,d)     *(u8*)((ulong_ptr)v+o)=(u8)d
#define svm_vmwrite16(v,o,d)    *(u16*)((ulong_ptr)v+o)=(u16)d
#define svm_vmwrite32(v,o,d)    *(u32*)((ulong_ptr)v+o)=(u32)d
#define svm_vmwrite64(v,o,d)    *(u64*)((ulong_ptr)v+o)=(u64)d

#define svm_vmcopy8(d,s,o)      svm_vmwrite8(d,o,svm_vmread8(s,o))
#define svm_vmcopy16(d,s,o)     svm_vmwrite16(d,o,svm_vmread16(s,o))
#define svm_vmcopy32(d,s,o)     svm_vmwrite32(d,o,svm_vmread32(s,o))
#define svm_vmcopy64(d,s,o)     svm_vmwrite64(d,o,svm_vmread64(s,o))

#define svm_vmread      svm_vmread64
#define svm_vmwrite     svm_vmwrite64
#define svm_vmcopy      svm_vmcopy64

#define svm_attrib(a)			(u16)(((a&0xFF)|((a&0xF000)>>4))&0xfff)
#define svm_attrib_inverse(a)	(u16)(((a&0xF00)<<4)|(a&0xFF))

#define max_core_count 64

typedef union _large_integer {
	struct {
		unsigned long low_part;
		long high_part;
	} DUMMYSTRUCTNAME;
	struct {
		unsigned long low_part;
		long high_part;
	} u;
	long long quad_part;
} large_integer;

#pragma once

typedef unsigned __int8		u8;
typedef unsigned __int16	u16;
typedef unsigned __int32	u32;
typedef unsigned __int64	u64;

typedef signed __int8		i8;
typedef signed __int16		i16;
typedef signed __int32		i32;
typedef signed __int64		i64;

typedef u8* u8p;
typedef u16* u16p;
typedef u32* u32p;
typedef u64* u64p;

typedef i8* i8p;
typedef i16* i16p;
typedef i32* i32p;
typedef i64* i64p;

using guest_virt_t = u64;
using guest_phys_t = u64;
using host_virt_t = u64;
using host_phys_t = u64;

#if defined(_amd64)
typedef u64 ulong_ptr;
typedef i64 long_ptr;
#else
typedef u32 ulong_ptr;
typedef i32 long_ptr;
#endif

typedef ulong_ptr* ulong_ptr_p;
typedef long_ptr* long_ptr_p;
typedef size_t* size_p;

typedef volatile u8		u8v;
typedef volatile u16	u16v;
typedef volatile u32	u32v;
typedef volatile u64	u64v;

typedef volatile i8		i8v;
typedef volatile i16	i16v;
typedef volatile i32	i32v;
typedef volatile i64	i64v;

typedef volatile ulong_ptr	vulong_ptr;
typedef volatile long_ptr	vlong_ptr;

typedef u8v* u8vp;
typedef u16v* u16vp;
typedef u32v* u32vp;
typedef u64v* u64vp;

typedef i8v* i8vp;
typedef i16v* i16vp;
typedef i32v* i32vp;
typedef i64v* i64vp;

typedef vulong_ptr* vulong_ptr_p;
typedef vlong_ptr* vlong_ptr_p;

enum class svm_error_t
{
	error_success,
	pml4e_not_present,
	pdpte_not_present,
	pde_not_present,
	pte_not_present,
	vmxroot_translate_failure,
	invalid_self_ref_pml4e,
	invalid_mapping_pml4e,
	invalid_host_virtual,
	invalid_guest_physical,
	invalid_guest_virtual,
	page_table_init_failed
};

enum attach_option : u64 {
	no_attach,
	attach
};

struct apic_bar_msr
{
	union
	{
		uint64_t value;
		struct
		{
			uint64_t reserved1 : 8;           // [0:7]
			uint64_t bootstrap_processor : 1;  // [8]
			uint64_t reserved2 : 1;           // [9]
			uint64_t x2apic_mode : 1;    // [10]
			uint64_t xapic_global : 1;   // [11]
			uint64_t apic_base : 24;           // [12:35]
		};
	};
};

extern "C" {
	NTSTATUS NTAPI MmCopyVirtualMemory(
		PEPROCESS SourceProcess,
		PVOID SourceAddress,
		PEPROCESS TargetProcess,
		PVOID TargetAddress,
		SIZE_T BufferSize,
		KPROCESSOR_MODE PreviousMode,
		PSIZE_T ReturnSize
	);

	NTSTATUS NTAPI ZwQuerySystemInformation(
		IN size_t SystemInformationClass,
		OUT PVOID SystemInformation,
		IN ULONG SystemInformationLength,
		OUT PULONG ReturnLength OPTIONAL
	);

	PPEB NTAPI PsGetProcessPeb(
		IN PEPROCESS Process
	);

	NTSTATUS NTAPI ZwDeviceIoControlFile(
		IN HANDLE  FileHandle,
		IN HANDLE  Event,
		IN PIO_APC_ROUTINE  ApcRoutine,
		IN PVOID  ApcContext,
		OUT PIO_STATUS_BLOCK  IoStatusBlock,
		IN ULONG  IoControlCode,
		IN PVOID  InputBuffer,
		IN ULONG  InputBufferLength,
		OUT PVOID  OutputBuffer,
		IN ULONG  OutputBufferLength
	);
}

typedef struct _SYSTEM_MODULE_ENTRY {
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} SYSTEM_MODULE_ENTRY, * PSYSTEM_MODULE_ENTRY;

typedef struct _SYSTEM_MODULE {
	PVOID 	Reserved1;
	PVOID 	Reserved2;
	PVOID 	ImageBaseAddress;
	ULONG 	ImageSize;
	ULONG 	Flags;
	unsigned short 	Id;
	unsigned short 	Rank;
	unsigned short 	Unknown;
	unsigned short 	NameOffset;
	unsigned char 	Name[MAXIMUM_FILENAME_LENGTH];
} SYSTEM_MODULE, * PSYSTEM_MODULE;

typedef struct _SYSTEM_MODULE_INFORMATION {
	ULONG                       ModulesCount;
	SYSTEM_MODULE_ENTRY         Modules[1];
	ULONG                       Count;
	SYSTEM_MODULE 	            Sys_Modules[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;

typedef struct _PEB_LDR_DATA {
	ULONG Length;
	BOOLEAN Initialized;
	PVOID SsHandle;
	LIST_ENTRY ModuleListLoadOrder;
	LIST_ENTRY ModuleListMemoryOrder;
	LIST_ENTRY ModuleListInitOrder;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
	BYTE           Reserved1[16];
	PVOID          Reserved2[10];
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, * PRTL_USER_PROCESS_PARAMETERS;

typedef VOID(WINAPI* PPS_POST_PROCESS_INIT_ROUTINE)(VOID);

typedef struct _PEB {
	BYTE                          Reserved1[2];
	BYTE                          BeingDebugged;
	BYTE                          Reserved2[1];
	PVOID                         Reserved3[2];
	PPEB_LDR_DATA                 Ldr;
	PRTL_USER_PROCESS_PARAMETERS  ProcessParameters;
	PVOID                         Reserved4[3];
	PVOID                         AtlThunkSListPtr;
	PVOID                         Reserved5;
	ULONG                         Reserved6;
	PVOID                         Reserved7;
	ULONG                         Reserved8;
	ULONG                         AtlThunkSListPtr32;
	PVOID                         Reserved9[45];
	BYTE                          Reserved10[96];
	PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
	BYTE                          Reserved11[128];
	PVOID                         Reserved12[1];
	ULONG                         SessionId;
} PEB, * PPEB;

typedef struct _LDR_DATA_TABLE_ENTRY {
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;  // in bytes
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;  // LDR_*
	USHORT LoadCount;
	USHORT TlsIndex;
	LIST_ENTRY HashLinks;
	PVOID SectionPointer;
	ULONG CheckSum;
	ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB_LDR_DATA64 {
	ULONG Length;
	UCHAR Initialized;
	ULONG64 SsHandle;
	LIST_ENTRY64 InLoadOrderModuleList;
	LIST_ENTRY64 InMemoryOrderModuleList;
	LIST_ENTRY64 InInitializationOrderModuleList;
	ULONG64 EntryInProgress;
} PEB_LDR_DATA64, * PPEB_LDR_DATA64;

typedef struct _LDR_DATA_TABLE_ENTRY64 {
	LIST_ENTRY64 InLoadOrderLinks;
	LIST_ENTRY64 InMemoryOrderLinks;
	LIST_ENTRY64 InInitializationOrderLinks;
	ULONG64 DllBase;
	ULONG64 EntryPoint;
	ULONG64 SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	LIST_ENTRY64 HashLinks;
	ULONG64 SectionPointer;
	ULONG64 CheckSum;
	ULONG64 TimeDateStamp;
	ULONG64 LoadedImports;
	ULONG64 EntryPointActivationContext;
	ULONG64 PatchInformation;
	LIST_ENTRY64 ForwarderLinks;
	LIST_ENTRY64 ServiceTagLinks;
	LIST_ENTRY64 StaticLinks;
	ULONG64 ContextInformation;
	ULONG64 OriginalBase;
	LARGE_INTEGER LoadTime;
} LDR_DATA_TABLE_ENTRY64, * PLDR_DATA_TABLE_ENTRY64;

typedef struct _PEB64 {
	UCHAR InheritedAddressSpace;
	UCHAR ReadImageFileExecOptions;
	UCHAR BeingDebugged;
	UCHAR Spare;
	UCHAR Padding0[4];
	ULONG64 Mutant;
	ULONG64 ImageBaseAddress;
	ULONG64/*PPEB_LDR_DATA64*/ Ldr;
	RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
} PEB64, * PPEB64;

typedef struct _mtrr_range_descriptor
{
	SIZE_T physical_base_address;
	SIZE_T physical_end_address;
	UCHAR  memory_type;
} mtrr_range_descriptor, * pmtrr_range_descriptor;

typedef struct _ssdt {
	LONG* ServiceTable;
	PVOID CounterTable;
	ULONG64 SyscallsNumber;
	PVOID ArgumentTable;
}_ssdt, * _pssdt;

typedef struct _syscall_info {
	SHORT syscall_number;
	const char* syscall_name;
	PVOID hook_function_address;
	PVOID* original_function_address;
}syscall_info, * psyscall_info;

inline _pssdt nt_table;
inline _pssdt win32k_table;

inline u32 windows_build_number = 0;

#define cdecl		__cdecl
#define stdcall		__stdcall
#define fastcall	__fastcall

#define align_at(n)		__declspec(align(n))

#define null	(void*)0
#define maxu8	0xff
#define maxu16	0xffff
#define maxu32	0xffffffff
#define maxu64	0xffffffffffffffff

#define windows_7 7600
#define windows_7_sp1 7601
#define windows_8 9200
#define windows_8_1 9600
#define windows_11 22000

#define windows_1803 17134
#define windows_1809 17763
#define windows_1903 18362
#define windows_1909 18363
#define windows_2004 19041
#define windows_20H2 19569
#define windows_21H1 20180

#define windows_10_version_threshold1 10240
#define windows_10_version_threshold2 10586

#define windows_10_version_redstone1 14393
#define windows_10_version_redstone2 15063
#define windows_10_version_redstone3 16299
#define windows_10_version_redstone4 17134
#define windows_10_version_redstone5 17763

#define image_file_name 0x5A8 
#define active_threads 0x5F0 
#define thread_list_head 0x5E0  
#define active_process_links 0x448

#define JMP_SIZE (14)
#define MAPPER_DATA_SIZE (JMP_SIZE + 7)

#pragma warning (disable : 4245)
#pragma warning (disable : 4390)
#pragma warning (disable : 6011)

extern "C" {
	uint16_t __read_cs();
	uint16_t __read_ss();
	uint16_t __read_ds();
	uint16_t __read_es();
	uint16_t __read_fs();
	uint16_t __read_gs();
	uint16_t __read_tr();
	uint16_t __read_ldtr();

	void __write_ds(uint16_t selector);
	void __write_es(uint16_t selector);
	void __write_fs(uint16_t selector);
	void __write_gs(uint16_t selector);
	void __write_tr(uint16_t selector);
	void __write_ldtr(uint16_t selector);
}

template <typename str_type, typename str_type_2>
__forceinline bool crt_strcmp(str_type str, str_type_2 in_str, bool two) {
	if (!str || !in_str)
		return false;

	wchar_t c1, c2;
#define to_lower(c_char) ((c_char >= 'A' && c_char <= 'Z') ? (c_char + 32) : c_char)

	do {
		c1 = *str++; c2 = *in_str++;
		c1 = to_lower(c1); c2 = to_lower(c2);

		if (!c1 && (two ? !c2 : 1))
			return true;

	} while (c1 == c2);

	return false;
}

typedef bool (*callback)(PVOID ctx);

typedef struct page_context {
	bool is_execution;
	bool is_write;
	bool is_present;
	u64 guest_pa;
	u64 host_pa;
};

extern "C" u8 __ImageBase;

// Defines includes
#include "svm/defines_ctx/cpu.hpp"
#include "svm/defines_ctx/ntdef/def.h"
#include "svm/defines_ctx/bitset.hpp"
#include "svm/defines_ctx/memory-data.hpp"
#include "svm/defines_ctx/npt-data.hpp"
#include "svm/defines_ctx/descriptors-info.hpp"
#include "svm/defines_ctx/vmcb.hpp"
#include "svm/defines_ctx/msr-data.hpp"
#include "svm/defines_ctx/vprocessor-data.hpp"
#include "svm/defines_ctx/spin-lock.hpp"

// Utilities includes
#include "svm/utilities_ctx/memory/memory.hpp"
#include "svm/utilities_ctx/memory/page_table/page_table.hpp"
#include "svm/utilities_ctx/memory/memory_access/memory_access.hpp"
#include "svm/utilities_ctx/npt/npt.hpp"
#include "svm/utilities_ctx/disassembler/disassembler.hpp"
#include "svm/utilities_ctx/debug/debug.hpp"
#include "svm/utilities_ctx/msr/msr.hpp"
#include "svm/utilities_ctx/hook/hook.hpp"
#include "svm/utilities_ctx/utilities.hpp"
#include "svm/utilities_ctx/tpm/tpm.hpp"
#include "svm/utilities_ctx/memory/hide_page/hide_page.hpp"
#include "svm/utilities_ctx/logger/logger.hpp"
#include "svm/utilities_ctx/critical_protector/critical_protector.hpp"

// Defines includes
#include "svm/defines_ctx/virtual-memory.hpp"
#include "svm/defines_ctx/ia32.hpp"
#include "svm/defines_ctx/amd.hpp"
#include "svm/defines_ctx/vmexit.hpp"

// Handler includes
#include "svm/handlers_ctx/cpuid/cpuid_svm.hpp"
#include "svm/handlers_ctx/default/default_svm.hpp"
#include "svm/handlers_ctx/msr/msr_svm.hpp"
#include "svm/handlers_ctx/npf/npf_svm.hpp"
#include "svm/handlers_ctx/rdtsc/rdtsc_svm.hpp"
#include "svm/handlers_ctx/ud/ud_svm.hpp"
#include "svm/handlers_ctx/vmmcall/vmmcall_svm.hpp"
#include "svm/handlers_ctx/debug/debug_svm.hpp"
#include "svm/handlers_ctx/cr/cr_svm.hpp"
#include "svm/handlers_ctx/xsetbv/xsetbv_svm.hpp"

#include "svm/svm.hpp"
