#pragma once

/*
 * bitfield.h - bitfield definitions for i.MXRT 10xx peripherals
 */

#include <sys/lib/bitfield.h>

namespace bitfield {

/* single bit definition matches bitfield library */
template<typename StorageType, typename DataType, unsigned Bit>
using imxbit = bit<StorageType, DataType, Bit>;

/* bit ranges are inclusive, StartBit=MSB, EndBit=LSB */
template<typename StorageType, typename DataType, unsigned StartBit, unsigned EndBit>
using imxbits = bits<StorageType, DataType, EndBit, StartBit - EndBit + 1>;

}
