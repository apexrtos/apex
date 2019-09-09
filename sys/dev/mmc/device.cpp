#include "device.h"

#include "host.h"

namespace mmc {

/*
 * device::device
 */
device::device(host *h, unsigned tuning_cmd_index)
: h_{h}
, tuning_cmd_index_{tuning_cmd_index}
{ }

/*
 * device::~device
 */
device::~device()
{ }

/*
 * device::tuning_cmd_index
 */
unsigned
device::tuning_cmd_index() const
{
	return tuning_cmd_index_;
}

/*
 * device::mode
 */
device::mode_t
device::mode() const
{
	return v_mode();
}

}
