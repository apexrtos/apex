#pragma once

/*
 * USB Gadget CDC ACM (Abstract Control Model) Function
 */

#include <dev/usb/class/cdc_pstn.h>
#include <dev/usb/gadget/function.h>
#include <dev/usb/string_descriptor.h>
#include <sync.h>
#include <vector>

struct tty;

namespace usb::gadget {

class device;
class transaction;
class udc;

class cdc_acm final : public function {
public:
	cdc_acm(gadget::udc &);
	~cdc_acm();

	void tx_queue();
	void rx_queue();
	void flush_output();
	void flush_input();

private:
	int v_configure(std::string_view) override;
	int v_init(device &) override;
	void v_reset() override;
	int v_start(Speed) override;
	void v_stop() override;
	setup_result v_process_setup(const setup_request &, transaction &) override;
	size_t v_sizeof_descriptors(Speed) const override;
	size_t v_write_descriptors(Speed, std::span<std::byte>) override;

	void rx_done(transaction *, int status);
	void tx_done(transaction *, int status);

	tty *t_;
	bool running_;
	string_descriptor function_;
	cdc::pstn::line_coding line_coding_;

	std::vector<std::unique_ptr<transaction>> tx_;
	std::vector<std::unique_ptr<transaction>> rx_;

	a::mutex lock_;
};

}
