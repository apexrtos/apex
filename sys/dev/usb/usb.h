#pragma once

namespace usb {

/*
 * Supported USB speeds.
 */
enum class Speed {
	Low,	    /* USB 1.0 187.5 kB/s */
	Full,	    /* USB 1.0 1.5 MB/s */
	High,	    /* USB 2.0 60 MB/s */
};

/*
 * Maximum packet length for data transactions on default control pipe.
 */
inline constexpr auto
control_max_packet_len(const Speed spd)
{
	switch (spd) {
	case Speed::Low: return 8;
	case Speed::Full: return 64;
	case Speed::High: return 64;
	}
	__builtin_unreachable();
}

/*
 * Setup packets are always 8 bytes.
 */
inline constexpr auto setup_packet_len = 8;

/*
 * Maximum packet length on bulk endpoints.
 */
inline constexpr auto
bulk_max_packet_len(const Speed spd)
{
	switch (spd) {
	case Speed::Low: return 0;	/* low speed doesn't support bulk */
	case Speed::Full: return 64;
	case Speed::High: return 512;
	}
	__builtin_unreachable();
}

/*
 * Maximum packet length on interrupt endpoints.
 */
inline constexpr auto
interrupt_max_packet_len(const Speed spd)
{
	switch (spd) {
	case Speed::Low: return 8;
	case Speed::Full: return 64;
	case Speed::High: return 1024;
	}
	__builtin_unreachable();
}

}
