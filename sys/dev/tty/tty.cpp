#include "tty.h"

#include "buffer_queue.h"
#include <access.h>
#include <arch.h>
#include <conf/config.h>
#include <debug.h>
#include <device.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fs/file.h>
#include <fs/util.h>
#include <memory>
#include <mutex>
#include <sch.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/ttydefaults.h>
#include <task.h>
#include <termios.h>
#include <unistd.h>
#include <wait.h>

namespace {

namespace flags {
	inline constexpr auto cook_input = 0x1;
	inline constexpr auto rx_blocked_on_tx_full = 0x2;
	inline constexpr auto tx_stopped = 0x4;
	inline constexpr auto rx_overflow = 0x8;

};

bool is_ctrl(char c)
{
	return c < 32 || c == 0x7f;
}

}

/*
 * tty - tty instance
 *
 * Locking must follow the following protocol:
 *
 * state_lock_ -> rxq_lock_ -> txq_lock_
 */
class tty {
public:
	tty(size_t, size_t, std::unique_ptr<phys>, size_t,
	    std::unique_ptr<phys>, tty_tproc, tty_oproc, tty_iproc, tty_fproc,
	    void *);

	/* interface to filesystem */
	int open(file *);
	int close(file *);
	ssize_t read(file *, std::span<std::byte>);
	ssize_t write(file *, std::span<const std::byte>);
	int ioctl(file *, u_long, void *);
	void terminate();

	/* interface to drivers */
	void *driver_data();
	char *rx_getbuf();
	void rx_putbuf(char *, size_t);
	void rx_putc(char);
	void rx_overflow();
	int tx_getc();
	size_t tx_getbuf(size_t, const void **);
	bool tx_empty();
	void tx_advance(size_t);
	void tx_complete();

	void set_device(device *);
	::device *device();

private:
	size_t output(char);
	size_t output(const char *, size_t, bool);
	bool rubout(char);
	bool echo(char);
	void flush(int);
	void tx_start();
	int tx_wait();
	void cook();
	void set_termios(const termios &t);

	::device *dev_;		    /* device handle */

	event input_;		    /* input buffer ready */
	event output_;		    /* output buffer ready */
	event complete_;	    /* output complete */

	std::atomic_ulong flags_;   /* tty flags */

	/* tty state */
	a::spinlock state_lock_;
	size_t open_;		    /* open count */
	pid_t pgid_;		    /* foreground process group */
	termios termios_;	    /* termios state */
	winsize winsize_;	    /* window size */
	size_t column_;		    /* output column */
	size_t canon_column_;	    /* column at start of canonical line */

	/* transmit queue */
	a::spinlock_irq txq_lock_;
	circular_buffer_wrapper<char> txq_;
	circular_buffer_wrapper<char>::iterator txq_pos_;
	std::unique_ptr<phys> txq_pages_;

	/* receive buffer queue */
	a::spinlock_irq rxq_lock_;
	buffer_queue rxq_;
	buffer_queue::iterator rxq_processed_;
	buffer_queue::iterator rxq_pending_;
	buffer_queue::iterator rxq_cooked_;

	/* DPC for processing received data */
	dpc rx_dpc_;
	void rx_process();
	void rx_dpc();
	static void rx_dpc_fn(void *);

	/* static initialisation data */
	void *const driver_data_;	/* private driver data */
	const tty_oproc oproc_;		/* routine to start output */
	const tty_iproc iproc_;		/* routine to start input */
	const tty_fproc fproc_;		/* routine to flush queues */
	const tty_tproc tproc_;		/* routine to configure hardware */
};

/*
 * tty::tty
 */
tty::tty(size_t rx_bufcnt, size_t rx_bufsiz, std::unique_ptr<phys> rxp,
    size_t tx_bufsiz, std::unique_ptr<phys> txp, tty_tproc tproc,
    tty_oproc oproc, tty_iproc iproc, tty_fproc fproc, void *driver_data)
: dev_{}
, flags_{}
, open_{}
, pgid_{}
, column_{}
, canon_column_{}
, txq_{tx_bufsiz, static_cast<char *>(phys_to_virt(txp.get()))}
, txq_pos_{txq_.begin()}
, txq_pages_{std::move(txp)}
, rxq_{rx_bufcnt, rx_bufsiz, std::move(rxp)}
, rxq_processed_{rxq_.begin()}
, rxq_pending_{rxq_.begin()}
, rxq_cooked_{rxq_.begin()}
, rx_dpc_{}
, driver_data_{driver_data}
, oproc_{oproc}
, iproc_{iproc}
, fproc_{fproc}
, tproc_{tproc}
{
	event_init(&input_, "TTY input", event::ev_IO);
	event_init(&output_, "TTY output", event::ev_IO);
	event_init(&complete_, "TTY complete", event::ev_IO);

	termios t{};
	t.c_iflag = TTYDEF_IFLAG;
	t.c_oflag = TTYDEF_OFLAG;
	t.c_cflag = TTYDEF_CFLAG | TTYDEF_SPEED;
	t.c_lflag = TTYDEF_LFLAG;
	/* VSWTC, VDISCARD are not supported */
	t.c_cc[VINTR] = CINTR;
	t.c_cc[VQUIT] = CQUIT;
	t.c_cc[VERASE] = CERASE;
	t.c_cc[VKILL] = CKILL;
	t.c_cc[VEOF] = CEOF;
	t.c_cc[VTIME] = CTIME;		/* REVISIT: not yet supported */
	t.c_cc[VMIN] = CMIN;		/* REVISIT: not yet supported */
	t.c_cc[VSTART] = CSTART;
	t.c_cc[VSTOP] = CSTOP;
	t.c_cc[VSUSP] = CSUSP;
	t.c_cc[VEOL] = CEOL;
	t.c_cc[VREPRINT] = CREPRINT;	/* REVISIT: not yet supported */
	t.c_cc[VWERASE] = CWERASE;	/* REVISIT: not yet supported */
	t.c_cc[VLNEXT] = CLNEXT;	/* REVISIT: not yet supported */
	/* VEOL2 default is 0 */

	std::lock_guard sl{state_lock_};
	set_termios(t);
}

/*
 * tty::open - open a tty handle
 */
int
tty::open(file *f)
{
	task *t = task_cur();

	std::lock_guard sl{state_lock_};

	if (!(f->f_flags & O_NOCTTY) && t->sid == task_pid(t))
		pgid_ = task_pid(t);

	if ((open_++ == 0) && tproc_)
		if (int err = tproc_(this, termios_.c_cflag); err < 0)
			return err;

	return 0;
}

/*
 * tty::close - close a tty handle
 */
int
tty::close(file *f)
{
	std::lock_guard sl{state_lock_};

	if (!--open_)
		pgid_ = 0;

	return 0;
}

/*
 * tty::read - read data from a tty
 */
ssize_t
tty::read(file *f, std::span<std::byte> buf)
{
	auto rx_avail = [&] {
		return rxq_cooked_ != rxq_.begin();
	};

	/*
	 * REVISIT: must call u_access_suspend/u_access_resume unconditionally
	 * in case handling a previous iov entry called u_access_suspend as the
	 * task address space can change while access is suspended
	 */
	auto ua = u_access_suspend();
	if (auto r = u_access_resume(ua, data(buf), size(buf), PROT_WRITE); r)
		return r;

	std::unique_lock sl{state_lock_};
	std::unique_lock rl{rxq_lock_};

	/* wait for input */
	while (!rx_avail()) {
		/*
		 * REVISIT: The settings of MIN (c_cc[VMIN]) and TIME
		 * (c_cc[VTIME]) should be used to determine the
		 * circumstances in which a read(2) completes
		 */
		if (auto r = sch_prepare_sleep(&input_, 0); r)
			return r;
		rl.unlock();
		sl.unlock();
		auto ua = u_access_suspend();
		auto rc = sch_continue_sleep();
		if (auto r = u_access_resume(ua, data(buf), size(buf), PROT_WRITE); r)
			rc = r;
		if (rc)
			return rc;
		sl.lock();
		rl.lock();
	}

	size_t count;
	auto it = rxq_.begin();
	const auto end = rxq_cooked_;

	if (termios_.c_lflag & ICANON) {
		const auto &cc = termios_.c_cc;
		/* split input on '\n', cc[VEOL], cc[VEOL2] and cc[VEOF], do
		   not pass cc[VOEF] as input */
		count = 0;
		char *cbuf = reinterpret_cast<char *>(data(buf));
		while (count < size(buf) && it < end) {
			const char c = *it++;
			if (c == cc[VEOF])
				break;
			++count;
			*cbuf++ = c;
			if (c == '\n' || c == cc[VEOL] || c == cc[VEOL2])
				break;
		}
	} else {
		count = std::min<size_t>(size(buf), end - it);
		rxq_.copy(data(buf), count);
		it += count;
	}

	/* trim empty buffers */
	rxq_.trim_front(it);
	const auto bufavail = !rxq_.bufpool_empty();
	rl.unlock();
	sl.unlock();

	/* notify if buffers available */
	if (bufavail && iproc_)
		iproc_(this);

	return count;
}

/*
 * tty::write - write data to a tty
 */
ssize_t
tty::write(file *f, std::span<const std::byte> buf)
{
	const auto len{size(buf)};

	auto rval = [&](int rc) {
		return len - size(buf) ?: rc;
	};

	/*
	 * REVISIT: must call u_access_suspend/u_access_resume unconditionally
	 * in case handling a previous iov entry called u_access_suspend as the
	 * task address space can change while access is suspended
	 */
	auto ua = u_access_suspend();
	if (auto r = u_access_resume(ua, data(buf), size(buf), PROT_READ); r)
		return r;

	while (!empty(buf)) {
		std::unique_lock sl{state_lock_};
		buf = buf.subspan(output(
			reinterpret_cast<const char *>(data(buf)), size(buf),
			false));
		sl.unlock();

		tx_start();

		if (f->f_flags & O_NONBLOCK)
			return rval(-EAGAIN);

		if (!empty(buf)) {
			/* sleep until output queue drains */
			std::unique_lock tl{txq_lock_};
			if (auto r = sch_prepare_sleep(&output_, 0); r)
				return rval(r);
			auto txq_full = txq_.size() == txq_.capacity();
			tl.unlock();
			if (!txq_full) {
				sch_cancel_sleep();
				continue;
			}
			auto ua = u_access_suspend();
			auto rc = sch_continue_sleep();
			if (auto r = u_access_resume(ua, data(buf), size(buf), PROT_READ); r)
				rc = r;
			if (rc)
				return rval(rc);
		}
	}
	return rval(0);
}

/*
 * tty::ioctl - control a tty
 */
int
tty::ioctl(file *f, u_long cmd, void *arg)
{
	switch (cmd) {
	case TCGETS: {
		std::lock_guard sl{state_lock_};
		memcpy(arg, &termios_, sizeof(termios_));
		break;
	}
	case TCSETSW:
	case TCSETSF: {
		auto ua = u_access_suspend();
		auto rc = tx_wait();
		if (auto r = u_access_resume(ua, arg, sizeof(termios),
		    PROT_WRITE); r)
			rc = r;
		if (rc)
			return rc;
		std::lock_guard sl{state_lock_};
		if (cmd == TCSETSF)
			flush(TCIFLUSH);
		set_termios(*static_cast<termios *>(arg));
		if (tproc_)
			return tproc_(this, termios_.c_cflag);
		break;
	}
	case TCSETS: {
		std::lock_guard sl{state_lock_};
		set_termios(*static_cast<termios *>(arg));
		if (tproc_)
			return tproc_(this, termios_.c_cflag);
		break;
	}
	case TIOCSPGRP: {
		std::lock_guard sl{state_lock_};
		memcpy(&pgid_, arg, sizeof(pgid_));
		break;
	}
	case TIOCGPGRP: {
		std::lock_guard sl{state_lock_};
		memcpy(arg, &pgid_, sizeof(pgid_));
		break;
	}
	case TCFLSH: {
		int iarg = reinterpret_cast<int>(arg);
		switch (iarg) {
		case TCIFLUSH:
		case TCOFLUSH:
		case TCIOFLUSH: {
			std::lock_guard sl{state_lock_};
			flush(iarg);
		}
		default:
			return DERR(-EINVAL);
		}
		break;
	}
	case TCSBRK: {
		if (arg == nullptr)
			return DERR(-ENOTSUP);
		return tx_wait();
	}
	case TCXONC: {
		int iarg = reinterpret_cast<int>(arg);
		switch (iarg) {
		case TCOOFF:
			/* suspend output */
			flags_ |= flags::tx_stopped;
			break;
		case TCOON:
			/* restart output */
			flags_ &= ~flags::tx_stopped;
			tx_start();
			break;
		case TCIOFF:
			/* transmit stop character */
		case TCION: {
			/* transmit start character */
			std::unique_lock sl{state_lock_};
			char c = termios_.c_cc[iarg == TCIOFF ? VSTOP : VSTART];
			sl.unlock();
			if (c == _POSIX_VDISABLE)
				break;
			if (int r = write(f, as_bytes(std::span{&c, 1})); r < 0)
				return r;
			break;
		}
		default:
			return DERR(-EINVAL);
		}
		break;
	}
	case TIOCGWINSZ: {
		std::lock_guard l{state_lock_};
		memcpy(arg, &winsize_, sizeof(winsize_));
		break;
	}
	case TIOCSWINSZ: {
		std::lock_guard sl{state_lock_};
		memcpy(&winsize_, arg, sizeof(winsize_));
		break;
	}
	case TIOCINQ: {
		std::lock_guard rl{rxq_lock_};
		*static_cast<int *>(arg) = rxq_cooked_ - rxq_.begin();
		break;
	}
	case TIOCOUTQ: {
		std::lock_guard tl{txq_lock_};
		*static_cast<int *>(arg) = txq_.size();
		break;
	}
	default:
		return DERR(-ENOTSUP);
	}
	return 0;
}

/*
 * tty::terminate - terminate all operations running on tty
 */
void
tty::terminate()
{
	sch_wakeup(&input_, -ENODEV);
	sch_wakeup(&output_, -ENODEV);
	sch_wakeup(&complete_, -ENODEV);
}

/*
 * tty::data - get hardware driver specific tty data
 */
void *
tty::driver_data()
{
	return driver_data_;
}

/*
 * tty::rx_getbuf - get a buffer from tty to fill with raw character data
 *
 * Interrupt safe.
 */
char *
tty::rx_getbuf()
{
	std::lock_guard rl{rxq_lock_};
	return rxq_.bufpool_get();
}

/*
 * tty::rx_putbuf - return buffer from hardware driver to tty
 *
 * Interrupt safe.
 */
void
tty::rx_putbuf(char *buf, size_t len)
{
	std::unique_lock rl{rxq_lock_};
	rxq_.push(buf, len);
	rl.unlock();

	cook();
}

/*
 * tty::rx_putc - put a received character from hardware driver into tty
 *
 * Interrupt safe.
 */
void
tty::rx_putc(char c)
{
	std::unique_lock rl{rxq_lock_};
	const auto overflow = !rxq_.push(c);
	rl.unlock();

	if (overflow) {
		rx_overflow();
		return;
	}

	cook();
}

/*
 * tty::rx_overflow - notify tty of hardware receive overflow
 *
 * Interrupt safe.
 */
void
tty::rx_overflow()
{
	if (!(flags_ & flags::rx_overflow))
		dbg("tty: overflow!\n");
	flags_ |= flags::rx_overflow;

	cook();
}

/*
 * tty::tx_getc - get a character from tty for transmission
 *
 * Returns < 0 if there are no more characters to transmit.
 *
 * Interrupt safe.
 */
int
tty::tx_getc()
{
	std::unique_lock tl{txq_lock_};
	if (txq_.empty())
		return -1;
	const auto ret = txq_.front();
	txq_.pop_front();
	if (txq_.size() <= txq_.capacity() / 2)
		sch_wakeone(&output_);
	tl.unlock();

	if (flags_ & flags::rx_blocked_on_tx_full) {
		flags_ &= ~flags::rx_blocked_on_tx_full;
		cook();
	}

	return ret;
}

/*
 * tty::tx_getbuf - get transmit buffer description from tty
 *
 * Returned buffer is limited to maxlen length.
 *
 * Interrupt safe.
 */
size_t
tty::tx_getbuf(const size_t maxlen, const void **buf)
{
	std::lock_guard tl{txq_lock_};
	if (txq_pos_ == txq_.end())
		return 0;
	auto len = std::min(txq_.linear(txq_pos_), maxlen);
	*buf = &*txq_pos_;
	txq_pos_ += len;
	return len;
}

/*
 * tty::tx_empty - test if transmit buffer is empty
 *
 * Interrupt safe.
 */
bool
tty::tx_empty()
{
	std::lock_guard tl{txq_lock_};
	return txq_pos_ == txq_.end();
}

/*
 * tty::tx_advance - notify tty that data has been transmitted and transmit
 *                   buffer can be reused
 *
 * Interrupt safe.
 */
void
tty::tx_advance(size_t count)
{
	std::unique_lock tl{txq_lock_};
	assert(txq_.begin() + count <= txq_pos_);
	txq_.erase(txq_.begin(), txq_.begin() + count);
	const auto wakeup = txq_.size() <= txq_.capacity() / 2;
	tl.unlock();

	if (wakeup)
		sch_wakeone(&output_);

	if (flags_ & flags::rx_blocked_on_tx_full) {
		flags_ &= ~flags::rx_blocked_on_tx_full;
		cook();
	}
}

/*
 * tty::tx_complete - notify tty that physical transmission has completed
 *
 * Interrupt safe.
 */
void
tty::tx_complete()
{
	sch_wakeup(&complete_, 0);
}

/*
 * tty::set_device - associate tty with device handle
 */
void
tty::set_device(::device *dev)
{
	assert(!dev_);
	dev_ = dev;
}

/*
 * tth::device - retrive tty device handle
 */
::device *
tty::device()
{
	return dev_;
}

/*
 * tty::output - process & queue data for transmission
 *
 * Caller must hold state_lock_.
 *
 * Returns number of bytes queued. If atomic output is requested there must be
 * enough space in the transmit queue for all required output bytes otherwise
 * no data is queued.
 */
size_t
tty::output(char c)
{
	return output(&c, 1, true);
}

size_t
tty::output(const char *buf, size_t len, bool atomic)
{
	state_lock_.assert_locked();
	std::lock_guard tl{txq_lock_};

	const auto lflag = termios_.c_lflag;
	const auto oflag = termios_.c_oflag;

	auto txq_remain = [&] {
		return txq_.capacity() - txq_.size();
	};

	if (!(lflag & ICANON)) {
		if (atomic && txq_remain() < len)
			return 0;
		const auto cp = std::min(len, txq_remain());
		txq_.insert(txq_.end(), buf, buf + cp);
		return cp;
	}

	auto write_s = [&](const char *s, size_t len, int n) {
		if (txq_remain() < len)
			return false;
		txq_.insert(txq_.end(), s, s + len);
		column_ += n;
		return true;
	};

	auto write_c = [&](char c, int n) {
		if (!txq_remain())
			return false;
		txq_.push_back(c);
		column_ += n;
		return true;
	};

	const auto prev_column = column_;
	const auto prev_txq_size = txq_.size();
	size_t pos;

	for (pos = 0; pos != len; ++pos, ++buf) {
		const auto c = *buf;
		switch (c) {
		case '\t': {
			/* expand tab */
			const auto s = 8 - (column_ & 7);
			if (oflag & XTABS) {
				if (!write_s("        ", s, s))
					goto out;
			} else {
				if (!write_c(c, s))
					goto out;
			}
			continue;
		}
		case '\n':
			/* translate newlines */
			if (oflag & ONLCR) {
				if (!write_s("\r\n", 2, -column_))
					goto out;
			} else {
				if (!write_c(c, -column_))
					goto out;
			}
			continue;
		}

		if (!write_c(c, 1))
			break;
	}

out:
	if (atomic && pos != len) {
		column_ = prev_column;
		txq_.erase(txq_.begin() + prev_txq_size, txq_.end());
		return 0;
	}

	if (rxq_pending_ == rxq_cooked_)
		canon_column_ = prev_column;

	return pos;
}

/*
 * tty::rubout - rubout printed terminal character
 *
 * Caller must hold state_lock_.
 */
bool
tty::rubout(const char c)
{
	state_lock_.assert_locked();
	rxq_lock_.assert_locked();

	const auto lflag = termios_.c_lflag;
	const auto oflag = termios_.c_oflag;

	if (!(lflag & ECHO) || !(lflag & ECHOE))
		return true;

	std::lock_guard tl{txq_lock_};

	auto erase = [&](const char *s, size_t len, size_t n) {
		if (txq_.capacity() - txq_.size() < len)
			return false;
		txq_.insert(txq_.end(), s, s + len);
		column_ -= std::min(n, column_);
		return true;
	};

	if (c == ' ')
		return erase("\b", 1, 1);

	if (c == '\t') {
		size_t col = canon_column_;
		for (auto it = rxq_cooked_; it != rxq_pending_ - 1; ++it) {
			if (*it == '\t')
				col += 8 - (col & 7);
			else if (is_ctrl(*it))
				col += 2;
			else
				++col;
		}
		const auto s = column_ - col;
		assert(column_ >= col);
		assert(s <= 8);
		if (oflag & XTABS)
			return erase("\b\b\b\b\b\b\b\b", s, s);
		return erase("\b", 1, s);
	}

	if (is_ctrl(c) && c != '\n' && c != '\t')
		return erase("\b\b  \b\b", 6, 2);

	return erase("\b \b", 3, 1);
}

/*
 * tty::echo - echo character to terminal
 *
 * Caller must hold state_lock_.
 */
bool
tty::echo(char c)
{
	state_lock_.assert_locked();

	const auto lflag = termios_.c_lflag;

	if (!(lflag & ECHO)) {
		if (c == '\n' && (lflag & ECHONL))
			return !!output('\n');
		return true;
	}

	if (is_ctrl(c) && c != '\n' && c != '\t') {
		const char buf[2]{'^', static_cast<char>(c + 0x40)};
		return !!output(buf, 2, true);
	}

	return !!output(c);
}

/*
 * tty::flush - flush data from input/output queues
 *
 * Caller must hold state_lock_.
 */
void
tty::flush(int io)
{
	state_lock_.assert_locked();

	if (fproc_)
		fproc_(this, io);

	if (io == TCIFLUSH || io == TCIOFLUSH) {
		/* flush input */
		std::lock_guard rl{rxq_lock_};

		rxq_.clear();
		rxq_processed_ = rxq_pending_ = rxq_cooked_ = rxq_.begin();

		flags_ &= ~flags::rx_overflow;

		/* use dpc to requeue rx buffers */
		sch_dpc(&rx_dpc_, rx_dpc_fn, this);
	}
	if (io == TCOFLUSH || io == TCIOFLUSH) {
		/* flush output */
		std::lock_guard tl{txq_lock_};

		txq_.clear();
		txq_pos_ = txq_.begin();

		if (flags_ & flags::rx_blocked_on_tx_full) {
			sch_dpc(&rx_dpc_, rx_dpc_fn, this);
			flags_ &= ~flags::rx_blocked_on_tx_full;
		}
	}
}

/*
 * tty::tx_start - start transmission
 */
void
tty::tx_start()
{
	if (!(flags_ & flags::tx_stopped) && oproc_)
		oproc_(this);
}

/*
 * tty::tx_wait - wait for transmission to complete
 */
int
tty::tx_wait()
{
	std::unique_lock tl{txq_lock_};
	return wait_event_interruptible_lock(complete_, tl, [&] {
		return txq_.empty();
	});
}

/*
 * tty::cook - take raw input data and cook if necessary
 *
 * Interrupt safe.
 */
void
tty::cook()
{
	/* cook input if necessary */
	const auto f = flags_.load();
	if (f & flags::cook_input) {
		if (!(f & flags::rx_blocked_on_tx_full))
			sch_dpc(&rx_dpc_, rx_dpc_fn, this);
		return;
	}

	/* otherwise pass data straight through */
	std::unique_lock rl{rxq_lock_};
	rxq_cooked_ = rxq_processed_ = rxq_pending_ = rxq_.end();
	rl.unlock();

	sch_wakeone(&input_);
}

/*
 * tty::set_termios - update termios state
 *
 * Caller must hold state_lock_.
 */
void
tty::set_termios(const termios &t)
{
	state_lock_.assert_locked();

	termios_ = t;
	auto lflag = termios_.c_lflag;
	auto iflag = termios_.c_iflag;
	if (lflag & (ECHO | ISIG | ICANON) || iflag & IXON)
		flags_ |= flags::cook_input;
	else
		flags_ &= ~flags::cook_input;
}

/*
 * tty::rx_process - perform input processing on received characters
 */
void
tty::rx_process()
{
	const auto lflag = termios_.c_lflag;
	const auto iflag = termios_.c_iflag;
	const auto &cc = termios_.c_cc;

	std::unique_lock rl{rxq_lock_};

	while (rxq_processed_ != rxq_.end()) {
		char c = *rxq_processed_++;

		/* IGNCR, ICRNL, INLCR */
		if (c == '\r') {
			if (iflag & IGNCR)
				continue;
			else if (iflag & ICRNL)
				c = '\n';
		} else if (c == '\n' && (iflag & INLCR))
			c = '\r';

		/* flow control */
		if (iflag & IXON) {
			/* stop (^S) */
			if (c == cc[VSTOP]) {
				/* if VSTART == VSTOP toggle */
				if (cc[VSTOP] == cc[VSTART])
					flags_ ^= flags::tx_stopped;
				else
					flags_ |= flags::tx_stopped;
				continue;
			}
			/* start (^Q) */
			if (c == cc[VSTART]) {
				flags_ &= ~flags::tx_stopped;
				continue;
			}
		}

		/* signals */
		if (lflag & ISIG) {
			/* quit (^C) */
			if (c == cc[VINTR] || c == cc[VQUIT]) {
				if (!(lflag & NOFLSH)) {
					rl.unlock();
					flush(TCIOFLUSH);
					rl.lock();
				}
				if (!echo(c) && !(flags_ & flags::rx_overflow))
					goto out;
				const int sig = (c == cc[VINTR])
				    ? SIGINT : SIGQUIT;
				if (pgid_ > 0)
					kill(-pgid_, sig);
				if (!(lflag & NOFLSH))
					goto out;
				continue;
			}
			/* suspend (^Z) */
			if (c == cc[VSUSP]) {
				if (!(lflag & NOFLSH)) {
					rl.unlock();
					flush(TCIOFLUSH);
					rl.lock();
				}
				if (!echo(c) && !(flags_ & flags::rx_overflow))
					goto out;
				const int sig = SIGTSTP;
				if (pgid_ > 0)
					kill(-pgid_, sig);
				if (!(lflag & NOFLSH))
					goto out;
				continue;
			}
		}

		/* prevent overflow from generating bad input */
		if (flags_ & flags::rx_overflow)
			continue;

		/* canonical input */
		if (lflag & ICANON) {
			/* erase (^H / ^?) or backspace */
			if ((lflag & ECHOE) &&
			    (c == cc[VERASE] || c == '\b')) {
				if (rxq_pending_ > rxq_cooked_) {
					auto prev = rxq_pending_ - 1;
					if (!rubout(*prev))
						goto out;
					rxq_pending_ = prev;
				}
				continue;
			}
			if ((lflag & ECHOE) && c == cc[VWERASE]) {
				bool found_word = false;
				while (rxq_pending_ > rxq_cooked_) {
					auto prev = rxq_pending_ - 1;
					if (*prev == ' ' || *prev == '\t') {
						if (found_word)
							break;
					} else
						found_word = true;
					if (!rubout(*prev))
						goto out;
					rxq_pending_ = prev;
				}
				continue;
			}
			/* kill (^U) */
			if ((lflag & (ECHOK | ECHOKE)) && c == cc[VKILL]) {
				while (rxq_pending_ > rxq_cooked_) {
					auto prev = rxq_pending_ - 1;
					if (!rubout(*prev))
						goto out;
					rxq_pending_ = prev;
				}
				continue;
			}
		}

		if (!echo(c))
			goto out;

		rxq_.expand_if_no_overlap(rxq_pending_, rxq_processed_);
		if (*rxq_pending_ != c)
			*rxq_pending_ = c;
		++rxq_pending_;

		if (lflag & ICANON && (c == '\n' || c == cc[VEOF] ||
		    c == cc[VEOL] || c == cc[VEOL2]))
			rxq_cooked_ = rxq_pending_;
	}

out:
	/* any character restarts output */
	if (iflag & IXANY)
		flags_ &= ~flags::tx_stopped;

	/* restart output if necessary */
	tx_start();

	if (!(lflag & ICANON))
		rxq_cooked_ = rxq_pending_;
}

/*
 * tty::rx_dpc - deferred procedure call for receive processing
 */
void
tty::rx_dpc()
{
	std::unique_lock sl{state_lock_};

	rx_process();

	/* trim empty buffers & check if data is ready */
	std::unique_lock rl{rxq_lock_};
	if (flags_ & flags::rx_overflow) {
		rxq_.free_buffers_after(rxq_cooked_);
		rxq_pending_ = rxq_processed_ = rxq_.end();
	} else if (rxq_processed_ != rxq_.end()) {
		std::lock_guard tl{txq_lock_};
		if (!txq_.empty())
			flags_ |= flags::rx_blocked_on_tx_full;
	} else {
		rxq_.free_buffers_after(rxq_pending_);
		rxq_processed_ = rxq_.end();
	}
	const auto bufavail = !rxq_.bufpool_empty();
	const auto dataavail = rxq_cooked_ != rxq_.begin();
	rl.unlock();
	sl.unlock();

	/* notify if buffer available */
	if (bufavail && iproc_)
		iproc_(this);

	/* wakeup threads waiting for input */
	if (dataavail)
		sch_wakeone(&input_);
}

/*
 * tty::rx_dpc_fn - deferred procedure call wrapper for receive processing
 */
void
tty::rx_dpc_fn(void *arg)
{
	static_cast<tty *>(arg)->rx_dpc();
}

namespace {

/*
 * tty_id - page ownership identifier for tty
 */
static char tty_id;

/*
 * tty_open - open a tty handle
 */
int
tty_open(file *f)
{
	tty *t = static_cast<tty *>(f->f_data);
	return t->open(f);
}

/*
 * tty_close - close a tty handle
 */
int
tty_close(file *f)
{
	tty *t = static_cast<tty *>(f->f_data);
	return t->close(f);
}

/*
 * tty_read_iov - read data from a tty
 */
ssize_t
tty_read_iov(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [f](std::span<std::byte> buf, off_t offset) {
		return static_cast<tty *>(f->f_data)->read(f, buf);
	});
}

/*
 * tty_write_iov - write data to a tty
 */
ssize_t
tty_write_iov(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [f](std::span<const std::byte> buf, off_t offset) {
		return static_cast<tty *>(f->f_data)->write(f, buf);
	});
}

/*
 * tty_ioctl - control a tty
 */
int
tty_ioctl(file *f, u_long cmd, void *data)
{
	tty *t = static_cast<tty *>(f->f_data);
	return t->ioctl(f, cmd, data);
}

}

extern "C" {

/*
 * tty_create - create a tty device
 *
 * rx_bufsiz: receive buffer size, power of 2 <= PAGE_SIZE
 * rx_bufmin: minimum number of receive buffers
 * tproc: routine to configure hardware
 * oproc: routine to start output
 * iproc: routine to start input
 * fproc: routine to flush queues
 */
tty *
tty_create(const char *name, long attr, size_t rx_bufsiz, size_t rx_bufmin,
    tty_tproc tproc, tty_oproc oproc, tty_iproc iproc, tty_fproc fproc,
    void *driver_data)
{
	if (rx_bufsiz > PAGE_SIZE || PAGE_SIZE % rx_bufsiz)
		return (tty *)DERR(-EINVAL);
	const size_t rx_sz = ALIGNn(rx_bufsiz * rx_bufmin, PAGE_SIZE);

	std::unique_ptr<phys> rxp{page_alloc(rx_sz, attr, &tty_id),
	    {rx_sz, &tty_id}};
	std::unique_ptr<phys> txp{page_alloc(PAGE_SIZE, attr, &tty_id),
	    {PAGE_SIZE, &tty_id}};
	if (!rxp.get() || !txp.get())
		return (tty *)DERR(-ENOMEM);
	const size_t rx_bufcnt = rx_sz / rx_bufsiz;
	std::unique_ptr<tty> t{std::make_unique<tty>(rx_bufcnt, rx_bufsiz,
	    std::move(rxp), PAGE_SIZE, std::move(txp), tproc, oproc,
	    iproc, fproc, driver_data)};
	if (!t.get())
		return (tty *)DERR(-ENOMEM);

	device *dev;
	static constinit devio tty_io{
		.open = tty_open,
		.close = tty_close,
		.read = tty_read_iov,
		.write = tty_write_iov,
		.ioctl = tty_ioctl,
	};
	if (dev = device_create(&tty_io, name, DF_CHR, t.get()); !dev)
		return (tty *)DERR(-EINVAL);

	t->set_device(dev);

	return t.release();
}

/*
 * tty_destroy - destroy a tty device
 */
void
tty_destroy(tty *t)
{
	/* device_hide guarantees that no more threads can call tty_open,
	 * tty_close, tty_read, tty_write or tty_ioctl on this tty */
	device_hide(t->device());

	/* kill all active operations on this tty */
	while (device_busy(t->device())) {
		t->terminate();
		timer_delay(0);
	}

	/* destroy tty */
	device_destroy(t->device());
	delete t;
}

/*
 * tty_data - get hardware driver specific tty data
 *
 * Interrupt safe.
 */
void *
tty_data(tty *t)
{
	return t->driver_data();
}

/*
 * tty_speed - convert enumerated baud rate to integer
 */
long
tty_speed(tcflag_t b)
{
	switch (b & CBAUD) {
	case B0: return 0;
	case B50: return 50;
	case B75: return 75;
	case B110: return 110;
	case B134: return 134;
	case B150: return 150;
	case B200: return 200;
	case B300: return 300;
	case B600: return 600;
	case B1200: return 1200;
	case B1800: return 1800;
	case B2400: return 2400;
	case B4800: return 4800;
	case B9600: return 9600;
	case B19200: return 19200;
	case B38400: return 38400;
	case B57600: return 57600;
	case B115200: return 115200;
	case B230400: return 230400;
	case B460800: return 460800;
	case B500000: return 500000;
	case B576000: return 576000;
	case B921600: return 921600;
	case B1000000: return 1000000;
	case B1152000: return 1152000;
	case B1500000: return 1500000;
	case B2000000: return 2000000;
	case B2500000: return 2500000;
	case B3000000: return 3000000;
	case B3500000: return 3500000;
	case B4000000: return 4000000;
	default: return -1;
	}
}

/*
 * tty_rx_getbuf - get a buffer from tty to fill with raw character data
 *
 * Interrupt safe.
 */
char *
tty_rx_getbuf(tty *t)
{
	return t->rx_getbuf();
}

/*
 * tty_rx_putbuf - return buffer from hardware driver to tty
 *
 * Interrupt safe.
 */
void
tty_rx_putbuf(tty *t, char *buf, size_t pos)
{
	return t->rx_putbuf(buf, pos);
}

/*
 * tty_rx_putc - put a received character from hardware driver into tty
 *
 * Interrupt safe.
 */
void
tty_rx_putc(tty *t, char c)
{
	t->rx_putc(c);
}

/*
 * tty_rx_overflow - notify tty of hardware receive overflow
 *
 * Interrupt safe.
 */
void
tty_rx_overflow(tty *t)
{
	t->rx_overflow();
}

/*
 * tty_tx_getc - get a character from tty for transmission
 *
 * Returns < 0 if there are no more characters to transmit.
 *
 * Interrupt safe.
 */
int
tty_tx_getc(tty *t)
{
	return t->tx_getc();
}

/*
 * tty_tx_getbuf - get transmit buffer description from tty
 *
 * Returned buffer is limited to maxlen length.
 *
 * Interrupt safe.
 */
size_t
tty_tx_getbuf(tty *t, const size_t maxlen, const void **buf)
{
	return t->tx_getbuf(maxlen, buf);
}

/*
 * tty_tx_empty - test if transmit buffer is empty
 *
 * Interrupt safe.
 */
bool
tty_tx_empty(tty *t)
{
	return t->tx_empty();
}

/*
 * tty_tx_advance - notify tty that data has been transmitted and transmit
 *                  buffer can be reused
 *
 * Interrupt safe.
 */
void
tty_tx_advance(tty *t, size_t count)
{
	return t->tx_advance(count);
}

/*
 * tty_tx_complete - notify tty that physical transmission has completed
 *
 * Interrupt safe.
 */
void
tty_tx_complete(tty *t)
{
	t->tx_complete();
}

}
