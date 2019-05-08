#include "function.h"

#include <debug.h>
#include <dev/usb/gadget/configuration.h>
#include <string_utils.h>

namespace usb::gadget {

namespace {

/*
 * List of function descriptions
 */
std::list<detail::function_desc *> functions;

}

/*
 * function::function
 */
function::function(gadget::udc &udc, size_t endpoints, size_t interfaces)
: udc_{udc}
, endpoints_{endpoints}
, interfaces_{interfaces}
, endpoint_offset_{0}
, interface_offset_{0}
{ }

/*
 * function::~function
 */
function::~function()
{ }

/*
 * function::configure - parse configuration string for function
 */
int
function::configure(std::string_view s)
{
	return v_configure(s);
}

/*
 * function::init - initialise function
 */
int
function::init(device &d)
{
	return v_init(d);
}

/*
 * function::reset - reset function
 */
void
function::reset()
{
	v_reset();
}

/*
 * function::start - start function running at specified speed
 */
int
function::start(const Speed spd)
{
	return v_start(spd);
}

/*
 * function::stop - stop function
 */
void
function::stop()
{
	return v_stop();
}

/*
 * function::process_setup - attempt to handle setup request
 */
setup_result
function::process_setup(const setup_request &s, transaction &t)
{
	return v_process_setup(s, t);
}

/*
 * function::set_offsets - set endpoint & interface offset for function
 */
void
function::set_offsets(size_t endpoint, size_t interface)
{
	endpoint_offset_ = endpoint;
	interface_offset_ = interface;
}

/*
 * function::write_descriptors - write descriptors for function running at
 *				 specified speed
 */
size_t
function::write_descriptors(const Speed spd, std::span<std::byte> m)
{
	return v_write_descriptors(spd, m);
}

/*
 * function::udc - return reference to device controller
 */
gadget::udc &
function::udc()
{
	return udc_;
}

/*
 * function::endpoints - return number of endpoints required
 */
size_t
function::endpoints() const
{
	return endpoints_;
}

/*
 * function::interfaces - return number of interfaces
 */
size_t
function::interfaces() const
{
	return interfaces_;
}

/*
 * function::sizeof_descriptors - return size of descriptors for function
 *				  running at specified speed
 */
size_t
function::sizeof_descriptors(const Speed spd) const
{
	return v_sizeof_descriptors(spd);
}

/*
 * function::endpoint_offset - return endpoint offset for this function
 */
size_t
function::endpoint_offset() const
{
	return endpoint_offset_;
}

/*
 * function::interface_offset - return interface offset for this function
 */
size_t
function::interface_offset() const
{
	return interface_offset_;
}

/*
 * function::instantiate - return a new instance of this function
 */
std::shared_ptr<function>
function::instantiate(gadget::udc &udc, std::string_view name)
{
	for (auto &f : functions)
		if (f->name() == name)
			return f->construct(udc);
	return nullptr;
}

/*
 * function::add - add a new function to the function factory
 */
void
function::add(detail::function_desc *n)
{
	for (auto f : functions) {
		if (f->name() == n->name()) {
			dbg("usb::gadget::function::add duplicate function %.*s\n",
			    f->name().length(), f->name().data());
			return;
		}
	}
	functions.emplace_back(n);
}

namespace detail {

/*
 * function_desc::function_desc
 */
function_desc::function_desc(std::string_view name)
: name_{name}
{ }

/*
 * function_desc::~function_desc
 */
function_desc::~function_desc()
{ }

/*
 * function_desc::name - return function name
 */
const std::string&
function_desc::name() const
{
	return name_;
}

}

}
