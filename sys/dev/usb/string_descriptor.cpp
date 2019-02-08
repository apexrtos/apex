#include "string_descriptor.h"

#include <dev/usb/ch9.h>
#include <endian.h>
#include <uchar.h>

namespace usb {

string_descriptor::string_descriptor()
: index_{0}
{ }

string_descriptor::string_descriptor(std::string_view s)
{
	*this = s;
}

string_descriptor::string_descriptor(std::u16string_view s)
{
	*this = s;
}

string_descriptor &
string_descriptor::operator=(std::string_view s)
{
	data_.clear();
	data_.reserve(s.length() * 2 + 2);
	data_.emplace_back(static_cast<std::byte>(0));	/* length byte */
	data_.emplace_back(static_cast<std::byte>(ch9::DescriptorType::STRING));

	mbstate_t state{};
	union {
		char16_t c;
		std::byte b[2];
	};
	for (auto pos = begin(s);
	    auto rc = mbrtoc16(&c, &pos[0], end(s) - pos, &state);) {
		if (255 - data_.size() < 4)
			break;
		if (rc == (size_t)-2)	/* incomplete input */
			break;
		if (rc == (size_t)-1)	/* encoding error */
			break;
		if (rc == (size_t)-3)	/* output from earlier surrogate pair */
			rc = 0;
		c = htole16(c);
		data_.emplace_back(b[0]);
		data_.emplace_back(b[1]);
		pos += rc;
	}

	/* finalise last character if truncating */
	if (mbrtoc16(&c, nullptr, 0, &state) == (size_t)-3) {
		c = htole16(c);
		data_.emplace_back(b[0]);
		data_.emplace_back(b[1]);
	}

	data_[0] = static_cast<std::byte>(data_.size());

	return *this;
}

string_descriptor &
string_descriptor::operator=(std::u16string_view s)
{
	assert(s.length() <= 253);

	data_.clear();
	data_.reserve(s.length() * 2 + 2);
	data_.emplace_back(static_cast<std::byte>(s.length() * 2 + 2));
	data_.emplace_back(static_cast<std::byte>(ch9::DescriptorType::STRING));
	for (auto &cc : s) {
		union {
			char16_t c;
			std::byte b[2];
		};
		c = htole16(cc);
		data_.emplace_back(b[0]);
		data_.emplace_back(b[1]);
	}
	return *this;
}

void
string_descriptor::set_index(size_t index)
{
	index_ = index;
}

const std::vector<std::byte> &
string_descriptor::data() const
{
	return data_;
}

size_t
string_descriptor::index() const
{
	return index_;
}

bool
operator==(const string_descriptor &l, const string_descriptor &r)
{
	return l.data() == r.data();
}

}
