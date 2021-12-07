#pragma once

#include <cassert>

/*
 * bitfield.h - C++ typesafe bitfield
 *
 * This can be used to define hardware registers in a portable and typesafe
 * manner. For example:
 *
 * union hw_register {
 *         using S = uint32_t;
 *         struct { S r; };
 *         bitfield::bit<S, bool, 0> a_boolean_field;
 *         bitfield::bits<S, unsigned, 1, 3> a_3_bit_field;
 *         enum class values {
 *                 value1,
 *                 value2,
 *                 value3,
 *         };
 *         bitfield::bit<S, values, 4, 2> a_2_bit_enumerated_field;
 * };
 */

namespace bitfield {

template<typename StorageType, typename DataType, unsigned Offset, unsigned Size>
struct bits {
	using storage_type = StorageType;
	using data_type = DataType;
	static constexpr unsigned offset = Offset;
	static constexpr unsigned size = Size;
	static constexpr storage_type max = (storage_type{1} << size) - 1;
	static constexpr storage_type mask = max << offset;

	storage_type r;

	bits() = default;

	constexpr
	bits(data_type v)
	: r{static_cast<storage_type>(v) << offset}
	{ }

	constexpr
	operator data_type() const
	{
		return value();
	}

	constexpr
	data_type value() const
	{
		return static_cast<data_type>(r >> offset & max);
	}

	constexpr
	storage_type raw() const
	{
		return r & mask;
	}

	constexpr bits &
	operator=(data_type v)
	{
            assert(static_cast<storage_type>(v) <= max);
            r = (r & ~mask) | static_cast<storage_type>(v) << offset;
            return *this;
	}

	static_assert(sizeof(data_type) * 8 >= size);
	static_assert(sizeof(storage_type) * 8 >= offset + size);
};


template<typename StorageType, typename DataType, unsigned BitNumber>
using bit = bits<StorageType, DataType, BitNumber, 1>;

};
