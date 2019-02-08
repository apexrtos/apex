#include "init.h"

#include "cdc_acm.h"

using namespace usb::gadget;

void
usb_gadget_function_cdc_acm_init()
{
	function::add<cdc_acm>("cdc_acm");
}
