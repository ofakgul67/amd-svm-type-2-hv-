amd‑svm‑type‑2‑hv
Overview

amd‑svm‑type‑2‑hv is a research-oriented hypervisor implementation targeting AMD processors using SVM (Secure Virtual Machine) technology. It is designed as a Type‑2 hypervisor that runs alongside the host OS and provides mechanisms for:

Virtual machine control
Nested Page Table (NPT) manipulation
VM-exit management

The primary focus of this project is to explore hardware virtualization techniques on AMD platforms, particularly how nested paging and VM exit handling can be used for introspection, memory control, and hooking functionality at a low level.

This hypervisor is intended for research and educational purposes, not for production use.

Features
SVM initialization and VMX-like control unit setup
NPT (Nested Page Table) creation and management
Custom VM-exit handlers for monitoring critical CPU events
Memory control and guest access management
Basic logging framework for debugging and analysis
Multi-vCPU awareness with per-logical-thread state
Requirements
AMD CPU with SVM and NPT support
Windows or Linux host (kernel-mode code included)
UEFI/BIOS virtualization enabled (SVM)
Administrator/root privileges for driver or module loading
C++17 compatible toolchain
Building the Project

This repository uses a standard C++ build configuration. Adjust according to your environment.

On Windows (WDK/MSVC)
git clone https://github.com/ofakgul67/amd-svm-type-2-hv-.git
cd amd-svm-type-2-hv-
# Open .sln in Visual Studio with WDK
# Build Solution
On Linux (GCC/Clang)
git clone https://github.com/ofakgul67/amd-svm-type-2-hv-.git
cd amd-svm-type-2-hv-
mkdir build && cd build
cmake ..
make

Modify the build scripts if necessary to meet your environment needs.

Contributing

Contributions are welcome. This project is research-oriented, so clear documentation and tested patches are highly valued. When contributing, please:

Follow existing code style
Write clear commit messages
Include tests for new functionality
