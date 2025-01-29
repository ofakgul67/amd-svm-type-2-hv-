#pragma once
#include "../includes.h"

extern "C" bool handle_vmexit(svm::pvcpu_t vcpu, svm::pguest_registers_t guest_registers);
