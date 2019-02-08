#include "transaction.h"

namespace usb::gadget {

/*
 * transaction::transaction
 */
transaction::transaction()
: running_{0}
, buf_{nullptr}
, len_{0}
, zlt_{false}
{ }

/*
 * transaction::~transaction
 */
transaction::~transaction()
{ }

/*
 * transaction::clear - clear transaction state
 */
void
transaction::clear()
{
	assert(!running());
	buf_ = nullptr;
	len_ = 0;
	zlt_ = false;
	done_ = nullptr;
	finalise_ = nullptr;
}

/*
 * transaction::set_buf - assign transaction buffer
 */
void
transaction::set_buf(const void *buf, size_t len)
{
	assert(!running());
	buf_ = const_cast<void *>(buf);
	len_ = len;
}

/*
 * transaction::set_zero_length_termination - set zero length termination
 */
void
transaction::set_zero_length_termination(bool v)
{
	assert(!running());
	zlt_ = v;
}

/*
 * transaction::running - return transaction running state
 */
bool
transaction::running() const
{
	return running_;
}

/*
 * transaction::zero_length_termination - return zero length termination state
 */
bool
transaction::zero_length_termination() const
{
	return zlt_;
}

/*
 * transaction::buf - return transaction buffer
 */
void *
transaction::buf()
{
	return buf_;
}

/*
 * transaction::len - return transaction length
 */
size_t
transaction::len() const
{
	return len_;
}

/*
 * transaction::on_done - set function to run when transaction is retired
 */
void
transaction::on_done(std::function<void(transaction *, int)> fn)
{
	done_ = fn;
}

/*
 * transaction::on_finalise - set function to run when transaction is finalised
 */
void
transaction::on_finalise(std::function<void(transaction *, int)> fn)
{
	finalise_ = fn;
}

/*
 * transaction::started - notification from transaction implementation that
 *			  transaction has started
 */
void
transaction::started()
{
	running_ = true;
}

/*
 * transaction::retired - notification from transaction implementation that
 *			  transaction was retired
 */
void
transaction::retired(int status)
{
	running_ = false;
	if (done_)
		done_(this, status);
	if (finalise_)
		finalise_(this, status);
}

};
