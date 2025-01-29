#pragma once
#include "../includes.h"

enum class access : int8_t {
	read = 0,
	write = 1
};

struct base_msr // all MSRs *MUST* inherit from this (mostly to assure these functions are defined)
{
	template<class self>
	void load(self&& tis)
	{
		tis.load();
	}

	template<class self>
	void store(self&& tis)
	{
		tis.store();
	}
};

// The MSRPM (MSR Permission Map) is a bitmap (2 bits per MSR [R,W], 1 == operation is intercepted) that determines the access permissions for each MSR.
class alignas(0x1000) msrpm_t {
	union {
		struct {
			// 0x0 - 0x7FF
			bitset<0x800> vector1;

			// 0x800 - 0xFFF
			bitset<0x800> vector2;

			// 0x1000 - 0x17FF
			bitset<0x800> vector3;

			// 0x1800 - 0x1FFF
			bitset<0x800> vector4;
		};
		bitset<0x2000> vector;
	};

public:
	[[maybe_unused]] static constexpr u32 vector1_start = 0x0000'0000, vector1_end = 0x0000'1FFF;
	[[maybe_unused]] static constexpr u32 vector2_start = 0xC000'0000, vector2_end = 0xC000'1FFF;
	[[maybe_unused]] static constexpr u32 vector3_start = 0xC001'0000, vector3_end = 0xC001'1FFF;
	[[maybe_unused]] static constexpr u32 reserved_start = 0x4000'0000, reserved_end = 0x5000'0000; // unsure about the end value 

	void set(u32 msr, access access_bit, bool value = true) {
		bitset<0x800>* target = nullptr;
		if (msr >= vector3_start && msr <= vector3_end) {
			target = &vector3;
			msr -= vector3_start;
		}
		else if (msr >= vector2_start && msr <= vector2_end) {
			target = &vector2;
			msr -= vector2_start;
		}
		else if (msr <= vector1_end) {
			target = &vector1;
			msr -= vector1_start;
		}
		else {
			return;
		}

		target->set(msr * 2 + static_cast<int>(access_bit), value);
	}
};
static_assert(sizeof(msrpm_t) == 0x2000, "msrpm_t size mismatch");