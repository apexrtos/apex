#pragma once

#include <sys/uio.h>
#include <vector>
#include <span>

namespace usb::gadget {

/*
 * Description of a usb transaction
 *
 * A transaction will be executed as one or more transfers. Each transfer
 * will be of the maximum packet size for the endpoint except for the last
 * transfer which can be:
 * 1. maximum packet size (if the transaction is max packet size aligned)
 * 2. less than maximum packet size
 * 3. zero length (if transaction is max packet size aligned and the
 *    zero_length_termination option is set)
 *
 * When the transaction is retired the done and finalise events are run.
 */
class transaction {
public:
	transaction();
	virtual ~transaction();

	void clear();
	void set_buf(const void *, size_t);
	void set_zero_length_termination(bool);

	bool running() const;
	bool zero_length_termination() const;
	void *buf();
	size_t len() const;

	void on_done(std::function<void(transaction *, int)>);
	void on_finalise(std::function<void(transaction *, int)>);

protected:
	void started();
	void retired(int status);

private:
	bool running_;
	void *buf_;
	size_t len_;
	bool zlt_;
	std::function<void(transaction *, int)> done_;
	std::function<void(transaction *, int)> finalise_;
};

}
