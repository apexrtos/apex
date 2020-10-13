#ifndef dev_usb_gadget_udc_h
#define dev_usb_gadget_udc_h

/*
 * Generic USB Device Controller
 */

#include "device.h"
#include "setup_result.h"
#include <atomic>
#include <cstddef>
#include <dev/usb/setup_request.h>
#include <dev/usb/usb.h>
#include <memory>
#include <string>
#include <sync.h>

namespace usb::gadget {

class transaction;

class udc {
public:
	udc(std::string_view name, size_t endpoints);
	virtual ~udc();

	int start();
	void stop();
	ch9::DeviceState state();
	template<typename T, typename ...A> int set_device(A &&...);
	int set_device(std::unique_ptr<gadget::device>);
	std::shared_ptr<gadget::device> device();

	const std::string& name() const;
	size_t endpoints() const;

	int open_endpoint(size_t endpoint, ch9::Direction, ch9::TransferType,
	    size_t max_packet_len);
	void close_endpoint(size_t endpoint, ch9::Direction);
	int queue(size_t endpoint, ch9::Direction, transaction *);
	int flush(size_t endpoint, ch9::Direction);
	std::unique_ptr<transaction> alloc_transaction();

protected:
	void reset_irq();
	void bus_reset_irq();
	void port_change_irq(bool attached, Speed);
	void setup_request_irq(size_t endpoint, const ch9::setup_data &);
	void setup_aborted_irq(size_t endpoint);
	void ep_complete_irq(size_t endpoint, ch9::Direction);
	bool setup_requested(size_t endpoint);

private:
	virtual int v_start() = 0;
	virtual void v_stop() = 0;
	virtual int v_reset() = 0;
	virtual int v_bus_reset() = 0;
	virtual int v_port_change() = 0;
	virtual int v_open_endpoint(size_t endpoint, ch9::Direction,
	    ch9::TransferType, size_t max_packet_len) = 0;
	virtual void v_close_endpoint(size_t endpoint, ch9::Direction) = 0;
	virtual std::unique_ptr<transaction> v_alloc_transaction() = 0;
	virtual int v_queue(size_t endpoint, ch9::Direction, transaction *) = 0;
	virtual int v_queue_setup(size_t endpoint, ch9::Direction,
				  transaction *) = 0;
	virtual int v_flush(size_t endpoint, ch9::Direction) = 0;
	virtual void v_complete(size_t endpoint, ch9::Direction) = 0;
	virtual void v_set_stall(size_t endpoint, bool) = 0;
	virtual void v_set_stall(size_t endpoint, ch9::Direction, bool) = 0;
	virtual int v_get_stall(size_t endpoint, ch9::Direction) = 0;
	virtual void v_set_address(unsigned address) = 0;
	virtual void v_setup_aborted(size_t endpoint) = 0;

	a::mutex lock_;

	const std::string name_;
	const size_t endpoints_;
	bool running_;
	std::shared_ptr<gadget::device> device_;
	ch9::DeviceState state_;

	thread *th_;
	a::semaphore semaphore_;
	std::atomic_ulong events_;
	std::atomic_ulong complete_;
	std::atomic_bool attached_irq_;
	std::atomic<Speed> speed_irq_;
	std::atomic<ch9::setup_data> setup_data_irq_;

	std::unique_ptr<transaction> txn_;
	void *setup_buf_;
	static constexpr auto setup_buf_sz_ = 32;

	int queue_setup(ch9::Direction, transaction *);
	void process_setup(const setup_request &, Speed);
	setup_result dispatch_setup_request(const setup_request &, Speed);
	setup_result device_request(const setup_request &);
	setup_result interface_request(const setup_request &);
	setup_result endpoint_request(const setup_request &);
	setup_result device_get_status_request(const setup_request &);
	setup_result device_feature_request(const setup_request &, bool);
	setup_result device_set_address_request(const setup_request &);
	setup_result interface_get_status_request(const setup_request &);
	setup_result endpoint_get_status_request(const setup_request &);
	setup_result endpoint_feature_request(const setup_request &, bool);

	void process_events();
	void th_fn();
	static void th_fn_wrapper(void *);

public:
	static void add(udc *);
	static udc *find(std::string_view name);
};

/*
 * implementation details
 */
template<typename T, typename ...A>
int udc::set_device(A &&...a)
{
	return set_device(std::make_unique<T>(std::forward<A>(a)...));
}

}

#endif
