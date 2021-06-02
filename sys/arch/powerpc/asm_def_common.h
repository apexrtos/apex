#pragma once

#define text(t) __asm__("\n#__OUT__" t)
#define define_val(t, v) __asm__("\n#__OUT__#define " t " %0" :: "n" (v))
#define define_bit(t, v) __asm__("\n#__OUT__#define " t " (1 << %0)" :: "n" (v))
#define define_shift(t, v) __asm__("\n#__OUT__#define " t "(x) ((x) << %0)" :: "n" (v))
