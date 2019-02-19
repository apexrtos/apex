#ifndef dev_usb_gadget_function_h
#define dev_usb_gadget_function_h

/*
 * Abstract USB Composite Function
 */

#include <dev/usb/gadget/setup_result.h>
#include <dev/usb/usb.h>
#include <span>
#include <string>

namespace usb { class setup_request; }

namespace usb::gadget {

class transaction;
class udc;
class device;
namespace detail { class function_desc; }

class function {
public:
	function(gadget::udc &, size_t endpoints, size_t interfaces);
	virtual ~function();

	int configure(std::string_view);
	int init(device &);
	int finalise();
	void reset();
	int start(Speed);
	void stop();
	setup_result process_setup(const setup_request &, transaction &);
	void set_offsets(size_t endpoint, size_t interface);
	size_t write_descriptors(Speed, std::span<std::byte>);
	gadget::udc &udc();

	size_t endpoints() const;
	size_t interfaces() const;
	size_t sizeof_descriptors(Speed) const;
	size_t endpoint_offset() const;
	size_t interface_offset() const;

private:
	virtual int v_configure(std::string_view) = 0;
	virtual int v_init(device &) = 0;
	virtual int v_finalise() = 0;
	virtual void v_reset() = 0;
	virtual int v_start(Speed) = 0;
	virtual void v_stop() = 0;
	virtual setup_result v_process_setup(const setup_request &, transaction &) = 0;
	virtual size_t v_sizeof_descriptors(Speed) const = 0;
	virtual size_t v_write_descriptors(Speed, std::span<std::byte>) = 0;

	gadget::udc &udc_;
	const size_t endpoints_;
	const size_t interfaces_;
	size_t endpoint_offset_;
	size_t interface_offset_;

public:
	template<typename T> static void add(std::string_view name);
	static std::shared_ptr<function> instantiate(gadget::udc &,
	    std::string_view name);

	static void add(detail::function_desc *);
};

/*
 * Implementation details
 */
namespace detail {

class function_desc {
public:
	function_desc(std::string_view);
	virtual ~function_desc();

	const std::string& name() const;

	virtual std::unique_ptr<function> construct(gadget::udc &) const = 0;

private:
	const std::string name_;
};

template<typename T>
class function_T final : public function_desc {
public:
	function_T(std::string_view);

	std::unique_ptr<function> construct(gadget::udc &) const override;
};

template<typename T>
inline
function_T<T>::function_T(std::string_view name)
: function_desc{name}
{ }

template<typename T>
inline std::unique_ptr<function>
function_T<T>::construct(gadget::udc &udc) const
{
	return std::make_unique<T>(udc);
}

}

template<typename T>
inline void
function::add(std::string_view name)
{
	add(new detail::function_T<T>(name));
}

}

#endif
