#pragma once

#define __fast_data __attribute__((section(".fast_data")))
#define __fast_rodata __attribute__((section(".fast_rodata")))
#define __fast_text __attribute__((section(".fast_text")))
#define __fast_bss	__attribute__((section(".fast_bss")))
