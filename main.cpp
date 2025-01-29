#include "includes.h"
#pragma warning(disable : 6387)

// Driver unload routine
void driver_unload(PDRIVER_OBJECT driver_object) {
    UNREFERENCED_PARAMETER(driver_object);
    debug::panicf("Unloading driver...\n");
    virtualization::devirtualize();
}

// Driver entry point
NTSTATUS driver_entry(
    PVOID bootkit_base, 
    SIZE_T bootkit_size
) {
    // Initialize directly since we're already in IoInitSystem
    debug::panicf("Initializing core components...\n");
    utilities::get_windows_version();
    disassembler::get();
    memory::get();

    debug::panicf("Starting virtualization...\n");
    virtualization::virtualize();

    return STATUS_SUCCESS;
}