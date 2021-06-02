#pragma once

/*
 * bitfield.h - PowerPC bitfield support
 *
 * PowerPC special purpose registers are big endian and 64 bits wide with the
 * most significant bit numbered as 0 but with access to the least significant
 * 32 bits.
 *
 * The documented register:
 * [0 1 2 3 4 5 6 7 .... 32 33 34 35 36 37 38 39 .... 56 57 58 59 60 61 62 63]
 * What we actually access:
 *                      [32 33 34 35 36 37 38 39 .... 56 57 58 59 60 61 62 63]
 * The bit numbering we need:
 *			[31 30 29 28 27 26 25 24 ....  7  6  5  4  3  2  1  0]
 *
 * To avoid (too much) confusion we use bit numbering and bit ranges matching
 * the manuals and translate here.
 */

#include <lib/bitfield.h>

namespace bitfield {

template<typename StorageType, typename DataType, unsigned Bit>
using ppcbit = bit<StorageType, DataType, 63 - Bit>;

template<typename StorageType, typename DataType, unsigned StartBit, unsigned EndBit>
using ppcbits = bits<StorageType, DataType, 63 - EndBit, EndBit - StartBit + 1>;

}
