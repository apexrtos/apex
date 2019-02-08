#ifndef dev_usb_string_descriptor_h
#define dev_usb_string_descriptor_h

/*
 * USB string descriptor
 */

#include <string>
#include <vector>

namespace usb {

class string_descriptor final {
public:
	string_descriptor();
	string_descriptor(std::string_view);
	string_descriptor(std::u16string_view);

	string_descriptor &operator=(std::string_view);
	string_descriptor &operator=(std::u16string_view);

	void set_index(size_t);

	const std::vector<std::byte> &data() const;
	size_t index() const;

private:
	std::vector<std::byte> data_;
	size_t index_;
};

/*
 * operators
 */
bool operator==(const string_descriptor &, const string_descriptor &);

}

#endif
