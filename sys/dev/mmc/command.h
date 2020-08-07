#ifndef dev_mmc_command_h
#define dev_mmc_command_h

/*
 * MMC/SD Command
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <endian.h>
#include <sys/uio.h>

namespace mmc {

class command final {
public:
	/* Application-Specific Command (APP_CMD) */
	static constexpr auto ACMD = 0x80;

	enum class response_type {
		none,	    /* length, index, busy, crc */
		r1,	    /*     48,   yes,   no, yes */
		r1b,	    /*     48,   yes,  yes, yes */
		r2,	    /*    136,    no,   no,  no */
		r3,	    /*     48,    no,   no,  no */
		r4,	    /*     48,   yes,   no, yes */
		r5,	    /*     48,   yes,   no, yes */
		r5b,	    /*     48,   yes,  yes, yes */
		r6,	    /*     48,   yes,   no, yes */
		r7,	    /*     48,   yes,   no, yes */
	};

	enum class data_direction {
		none,
		host_to_device,
		device_to_host,
	};

	command(unsigned index, uint32_t argument, response_type);

	void setup_data_transfer(data_direction, size_t transfer_block_size,
	    const iovec *iov, size_t iov_off, size_t len, bool reliable_write);

	const iovec* iov() const;
	size_t iov_offset() const;
	data_direction data_direction() const;
	size_t data_size() const;
	size_t transfer_block_size() const;
	bool reliable_write() const;

	bool acmd() const;
	unsigned index() const;
	uint32_t argument() const;
	unsigned response_length() const;

	bool uses_data_lines() const;
	bool response_contains_index() const;
	bool response_crc_valid() const;
	bool busy_signalling() const;

	bool com_crc_error() const;

	std::array<std::byte, 16> &response();

private:
	const unsigned index_;
	const uint32_t argument_;
	const response_type response_type_;
	const iovec *iov_;
	size_t iov_off_;
	size_t data_size_;
	size_t transfer_block_size_;
	enum data_direction data_direction_;
	bool reliable_write_;

	/* Response data. Does not include first byte of on wire response. */
	alignas(4) std::array<std::byte, 16> response_;
};

}

#endif
