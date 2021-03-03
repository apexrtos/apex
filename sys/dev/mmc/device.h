#pragma once

/*
 * Generic MMC/SD Device
 */

#include <sys/types.h>
#include <variant>

namespace mmc {

class host;
namespace sd { enum class access_mode; }
namespace mmc { enum class device_type; }

class device {
public:
	using mode_t = std::variant<sd::access_mode, mmc::device_type>;

	device(host *, unsigned tuning_cmd_index);
	virtual ~device();

	unsigned tuning_cmd_index() const;
	mode_t mode() const;

protected:
	host *const h_;

private:
	const unsigned tuning_cmd_index_;

	virtual mode_t v_mode() const = 0;
};

}
