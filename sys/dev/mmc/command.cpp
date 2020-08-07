#include "command.h"

#include "extract.h"
#include <assert.h>

namespace mmc {

command::command(unsigned index, uint32_t argument, response_type rt)
: index_{index}
, argument_{argument}
, response_type_{rt}
, iov_{nullptr}
, iov_off_{0}
, data_size_{0}
, transfer_block_size_{0}
, data_direction_{data_direction::none}
, reliable_write_{false}
{ }

void
command::setup_data_transfer(enum data_direction d, size_t transfer_block_size,
    const struct iovec *iov, size_t iov_off, size_t len, bool reliable_write)
{
	data_direction_ = d;
	transfer_block_size_ = transfer_block_size;
	iov_ = iov;
	iov_off_ = iov_off;
	data_size_ = len;
	reliable_write_ = reliable_write;
}

const iovec *
command::iov() const
{
	return iov_;
}

size_t
command::iov_offset() const
{
	return iov_off_;
}

enum command::data_direction
command::data_direction() const
{
	return data_direction_;
}

size_t
command::data_size() const
{
	return data_size_;
}

size_t
command::transfer_block_size() const
{
	return transfer_block_size_;
}

bool
command::reliable_write() const
{
	return reliable_write_;
}

bool
command::acmd() const
{
	return index_ & ACMD;
}

unsigned
command::index() const
{
	return index_ & ~ACMD;
}

uint32_t
command::argument() const
{
	return argument_;
}

unsigned
command::response_length() const
{
	switch (response_type_) {
	case response_type::none:
		return 0;
	case response_type::r1:
	case response_type::r1b:
	case response_type::r3:
	case response_type::r4:
	case response_type::r5:
	case response_type::r5b:
	case response_type::r6:
	case response_type::r7:
		return 48;
	case response_type::r2:
		return 136;
	default:
		__builtin_unreachable();
	}
}

bool
command::uses_data_lines() const
{
	return data_size() || busy_signalling();
}

bool
command::response_contains_index() const
{
	switch (response_type_) {
	case response_type::r1:
	case response_type::r1b:
	case response_type::r4:
	case response_type::r5:
	case response_type::r5b:
	case response_type::r6:
	case response_type::r7:
		return true;
	case response_type::none:
	case response_type::r2:
	case response_type::r3:
		return false;
	default:
		__builtin_unreachable();
	}
}

bool
command::response_crc_valid() const
{
	switch (response_type_) {
	case response_type::r1:
	case response_type::r1b:
	case response_type::r4:
	case response_type::r5:
	case response_type::r5b:
	case response_type::r6:
	case response_type::r7:
		return true;
	case response_type::none:
	case response_type::r2:
	case response_type::r3:
		return false;
	default:
		__builtin_unreachable();
	}
}

bool
command::busy_signalling() const
{
	switch (response_type_) {
	case response_type::none:
	case response_type::r1:
	case response_type::r2:
	case response_type::r3:
	case response_type::r4:
	case response_type::r5:
	case response_type::r6:
	case response_type::r7:
		return false;
	case response_type::r1b:
	case response_type::r5b:
		return true;
	default:
		__builtin_unreachable();
	}
}

bool
command::com_crc_error() const
{
	switch (response_type_) {
	case response_type::r1:
	case response_type::r1b:
		uint32_t r;
		memcpy(&r, response_.data(), 4);
		r = be32toh(r);
		return bits(r, 23);
	case response_type::none:
	case response_type::r2:
	case response_type::r3:
	case response_type::r4:
	case response_type::r5:
	case response_type::r5b:
	case response_type::r6:
	case response_type::r7:
		return false;
	default:
		__builtin_unreachable();
	}
}

std::array<std::byte, 16> &
command::response()
{
	return response_;
}

}
