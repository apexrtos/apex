#pragma once

/*
 * bitfield.h - bitfield definitions for ARMv7-M registers
 */

#include <sys/lib/bitfield.h>

namespace bitfield {

/* single bit definition matches bitfield library */
template<typename StorageType, typename DataType, unsigned Bit>
using armbit = bit<StorageType, DataType, Bit>;

/* bit ranges are inclusive, StartBit=MSB, EndBit=LSB */
template<typename StorageType, typename DataType, unsigned StartBit, unsigned EndBit>
using armbits = bits<StorageType, DataType, EndBit, StartBit - EndBit + 1>;

}

