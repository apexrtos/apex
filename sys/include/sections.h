#ifndef sections_h
#define sections_h

#define __fast_data __attribute__((section(".fast_data")))
#define __fast_rodata __attribute__((section(".fast_rodata")))
#define __fast_text __attribute__((section(".fast_text")))

#ifdef __arm__
#define __fast_bss	__attribute__((section(".fast_bss, \"aw\", %nobits //")))
#else
#define __fast_bss	__attribute__((section(".fast_bss, \"aw\", @nobits //")))
#endif

#endif /* !sections_h */

