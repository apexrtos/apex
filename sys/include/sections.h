#ifndef sections_h
#define sections_h

#define __mem_fast_data __attribute__((section(".mem_fast_data")))

#ifdef __arm__
#define __mem_fast_bss	__attribute__((section(".mem_fast_bss, \"aw\", %nobits //")))
#else
#define __mem_fast_bss	__attribute__((section(".mem_fast_bss, \"aw\", @nobits //")))
#endif

#endif /* !sections_h */

