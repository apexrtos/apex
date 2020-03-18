/*
 * syslog.c - system log
 */

#include <debug.h>

#include <access.h>
#include <arch.h>
#include <compiler.h>
#include <device.h>
#include <errno.h>
#include <fcntl.h>
#include <fs/file.h>
#include <fs/util.h>
#include <kernel.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sync.h>
#include <unistd.h>
#include <wait.h>

/*
 * REVISIT: this code relies on well defined integer overflow behaviour which
 * is technically undefined behaviour in C, but OK under gcc.
 *
 * We may need to revisit this assumption in future when supporting clang. See
 * the following link for some more information.
 *
 * https://www.gnu.org/software/autoconf/manual/autoconf-2.69/html_node/Integer-Overflow.html
 */

/*
 * ent - system log entry
 */
struct ent {
	uint_fast64_t nsec;	/* timestamp in nanoseconds */
	long seq;		/* sequence number of message - safe to roll over */
	size_t len_term;	/* length of msg including null terminator, 0: wrap */
	long priority;		/* syslog facility and priority */
	char msg[];		/* message text */
};

static alignas(struct ent) char log[CONFIG_SYSLOG_SIZE];
static atomic_long log_first_seq = 1, log_last_seq;
static long clear_seq = 1;
static struct ent *head = (struct ent *)log;
static struct ent *tail = (struct ent *)log;
static struct ent *clear_ent = (struct ent *)log;

static struct event log_wait;
static struct spinlock lock;	/* REVISIT: may need init for SMP */
static struct kmsg_output {
	long seq;
	struct ent *ent;
} console_output = { .seq = 1, .ent = (struct ent *)log };

static int conlev = CONFIG_CONSOLE_LOGLEVEL + 1;
static const int min_conlev = LOG_WARNING + 1;

static void console_print_all(void);
static void (*log_output)(void) =
#if defined(CONFIG_EARLY_CONSOLE)
	console_print_all;
#else
	0;
#endif
static void (*console_print)(const char *, size_t) = early_console_print;

static_assert((sizeof(log) & (sizeof(log) - 1)) == 0,
    "SYSLOG_SIZE must be a power of 2");

static_assert(sizeof(log) >= 128,
    "SYSLOG_SIZE must be at least 128 bytes");

#if 0
/*
 * print direct to console: useful for debugging kmsg
 */
static int  __attribute__((format (printf, 1, 2)))
console_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char buf[256];
	int len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	early_console_print(buf, len);
	return len;
}
#endif

/*
 * panic_console_init - initialise panic console
 *
 * By default the early console is used.
 */
static void
panic_console_init_default(void)
{
	early_console_init();
}
weak_alias(panic_console_init_default, panic_console_init);

/*
 * panic_console_print - print to panic console
 *
 * By default the early console is used.
 */
static void
panic_console_print_default(const char *s, size_t len)
{
	early_console_print(s, len);
}
weak_alias(panic_console_print_default, panic_console_print);

/*
 * advance - move pointer to next entry
 */
static void
advance(struct ent **p)
{
	size_t len = (*p)->len_term;
	assert(len && len < sizeof(log));

	*p = (struct ent *)ALIGNn((char *)*p + sizeof(**p) + len,
	    alignof(**p));
	if ((char *)(*p + 1) >= (log + sizeof(log)))
		*p = (struct ent *)log;
}

/*
 * console_print_all - print new messages using console_print
 *
 * Must be interrupt save.
 */
static void
console_print_all(void)
{
	int len;
	char buf[256];

	while ((len = syslog_format(buf, sizeof(buf))) > 0)
		console_print(buf, len);
}

/*
 * log_trim - drop one msg from log buffer
 */
static void
log_trim(void)
{
	struct ent *entry = tail;
	++log_first_seq;
	advance(&tail);
	if (!tail->len_term) /* sentinel: wrap */
		tail = (struct ent *)log;
	entry->len_term = -1;
}

/*
 * syslog_begin - start writing a message
 */
static ssize_t
syslog_begin(struct ent **entry, long *seq, const size_t len)
{
	const size_t len_term = len + 1; /* terminating '\0' */
	const size_t ent_len = sizeof(struct ent) + len_term;
	if (ent_len > sizeof(log))
		return -ENOSPC;

	const int s = spinlock_lock_irq_disable(&lock);
	ssize_t rem;	/* linear free space in buffer */

check_len:
	rem = (char *)tail - (char *)head;
	if (rem > 0) {
		if (rem < ent_len) {
			log_trim();
			goto check_len;
		}
	} else {
		rem = log + sizeof(log) - (char *)head;
		if (rem < ent_len) {
			/*
			 * not enough space to end of log - wrap head
			 * back to start
			 */
			head->len_term = 0;	/* sentinel: wrap */
			head = (struct ent *)log;
			if (tail == head) /* catch wrap to completely full */
				log_trim();
			goto check_len;
		}
	}

	head->nsec = timer_monotonic();
	head->len_term = len_term;
	*entry = head;
	*seq = ++log_last_seq;
	advance(&head);
	if (tail == head) /* catch wrap to completely full */
		log_trim();

	spinlock_unlock_irq_restore(&lock, s);
	return len_term;
}

/*
 * syslog_end - finish writing a message
 */
static void
syslog_end(struct ent *entry, long seq, int priority)
{
	write_once(&entry->priority, priority);
	write_once(&entry->seq, seq);	     /* entry not read until seq is valid */

	void (*fn)(void) = read_once(&log_output);
	if (fn)
		fn();

	if (log_wait.sleepq.next) /* event initialised */
		sch_wakeup(&log_wait, 0);
}

/*
 * syslog_printf - print debug string to log & console
 *
 * Must be interrupt safe.
 *
 * No point locking the scheduler here as this function can be called from
 * interrupt handlers.
 */
int
syslog_printf(int level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = syslog_vprintf(level, fmt, ap);
	va_end(ap);
	return ret;
}

/*
 * syslog_vprintf - see syslog_printf
 */
int
syslog_vprintf(int level, const char *fmt, va_list ap)
{
	int len = vsnprintf(NULL, 0, fmt, ap); /* get length */
	if (len < 0)
		return len;

	struct ent *entry;
	long seq;
	ssize_t len_term = syslog_begin(&entry, &seq, len);
	if (len_term < 0)
		return len_term;

	vsnprintf(entry->msg, len_term, fmt, ap); /* can't fail? */
	syslog_end(entry, seq, LOG_MAKEPRI(LOG_KERN, level));
	return len;
}

/*
 * syslog_output - register function to be called when new messages available
 */
void
syslog_output(void (*fn)(void))
{
	write_once(&log_output, fn);
}

/*
 * syslog_format - format log messages for console
 *
 * Must be interrupt safe.
 */
int
syslog_format(char *buf, const size_t len)
{
	struct kmsg_output *kmsg = &console_output;
	int rem = len;

	/* lock-less read of log message */
	while ((log_last_seq - kmsg->seq) >= 0) { /* seq can rollover */
		struct ent entry = read_once(kmsg->ent);
		long overrun = log_first_seq - kmsg->seq;
		if (overrun > 0) {
			int n = snprintf(buf, rem, "*** missed %ld messages\n",
					 overrun);
			if (n < 0 || n >= rem)
				break;
			buf += n;
			rem -= n;
			kmsg->seq = log_first_seq;
			kmsg->ent = read_once(&tail);
			continue; /* process updated kmsg state */
		}
		if (entry.seq != kmsg->seq) { /* entry not valid */
			if (!entry.len_term) { /* sentinel: wrap */
				kmsg->ent = (struct ent *)log;
				continue;
			} else
				break; /* still being written... */
		}
		if (LOG_PRI(entry.priority) < conlev) {
			struct timeval tv;
			ns_to_tv(entry.nsec, &tv);
			int n = snprintf(buf, rem, "[%5lld.%06lld] ", tv.tv_sec, tv.tv_usec);
			size_t l = entry.len_term - 1;
			bool trunc = false;
			if (n < 0 || (n + l) > rem) {
				if (rem < len)
					break; /* won't fit in this buffer, try again */
				/* won't fit in empty buffer, truncate */
				l = rem - n;
				trunc = true;
			}

			memcpy(buf + n, kmsg->ent->msg, l); /* strip '\0' */
			if (log_first_seq - kmsg->seq > 0)
				continue; /* overrun while consuming entry, so skip */
			if (trunc)
				memcpy(buf + n + l - 4, "...\n", 4);

			l += n;
			buf += l;
			rem -= l;
		}

		++kmsg->seq;
		advance(&kmsg->ent);
	}

	return len - rem;
}

/*
 * kmsg_format - format one log message for userspace
 */
int
kmsg_format(char *buf, const size_t len, struct kmsg_output *kmsg)
{
	/* lock-less read of log message */
	struct ent entry = read_once(kmsg->ent);
	long overrun = log_first_seq - kmsg->seq;
	if (overrun > 0) {
	overrun:
		/* message we expect is gone: reset and return error */
		kmsg->seq = log_first_seq;
		kmsg->ent = read_once(&tail);
		return -EPIPE;	/* Linux compatible */
	}

	if (entry.seq != kmsg->seq) { /* entry not valid */
		if (!entry.len_term) /* sentinel: wrap */
			kmsg->ent = (struct ent *)log;
		else
			return -EAGAIN;	/* still being written... */
	}

	int n = snprintf(buf, len, "%lu,%lu,%llu,-; ",
			 entry.priority, entry.seq, entry.nsec / 1000);
	if (n < 0)
		return n;

	size_t ent_len = entry.len_term - 1;
	if (ent_len > (len - n))
		ent_len = len - n; /* won't fit: truncate */

	memcpy(buf + n, kmsg->ent->msg, ent_len); /* strip '\0' */
	if (log_first_seq - kmsg->seq > 0)
		goto overrun; /* overrun while consuming entry, so skip */

	++kmsg->seq;
	advance(&kmsg->ent);

	return ent_len + n;
}

/*
 * syslog_panic - flush log after panic
 *
 * Called with all interrupts disabled.
 */
void
syslog_panic(void)
{
	/* reset console output */
	console_output.seq = 1;	   /* will report how many msg are dropped if panic is partial */
	console_output.ent = (struct ent *)log;

	panic_console_init();
	panic_console_print("\n*** syslog_panic\n", 18);
	console_print = panic_console_print;
	console_print_all();
	syslog_output(console_print_all);
}

/*
 * sc_syslog - syslog system call
 */
int
sc_syslog(int type, char *buf, int len)
{
	static int prev_conlev = -1;
	int err;

	switch (type) {
	case 0: /* close */
	case 1: /* open */
		return 0;
	case 2: /* read */
		if ((err = u_access_begin()) < 0)
			return err;
		if (len < 0 || !u_access_ok(buf, len, PROT_WRITE)) {
			u_access_end();
			return DERR(-EINVAL);
		}
		/* TODO: implement */
		u_access_end();
		return DERR(-ENOSYS);
	case 3: /* read all */
		/* TODO: implement */
		return DERR(-ENOSYS);
	case 4: /* read clear */
		/* TODO: implement */
		return DERR(-ENOSYS);
	case 5: /* clear */
		clear_seq = log_last_seq + 1;
		clear_ent = read_once(&head);
		return 0;
	case 6: /* console off */
		if (prev_conlev == -1)
			prev_conlev = conlev;
		conlev = min_conlev;
		return DERR(-ENOSYS);
	case 7: /* console on */
		if (prev_conlev == -1)
			return 0;
		conlev = prev_conlev;
		prev_conlev = -1;
		return DERR(-ENOSYS);
	case 8: /* console level */
		if (len < 1 || len > 8)
			return DERR(-EINVAL);
		conlev = len < min_conlev ? min_conlev : len;
		prev_conlev = -1;
		return DERR(-ENOSYS);
	case 9: /* size unread */
		/* TODO: implement */
		return DERR(-ENOSYS);
	case 10: /* size buffer */
		return sizeof(log);
	default:
		return DERR(-EINVAL);
	}
}

/*
 * /dev/kmsg interface
 */
static int kmsg_open(struct file *file)
{
	struct kmsg_output *kmsg = malloc(sizeof (struct kmsg_output));
	if (!kmsg)
		return -ENOMEM;

	file->f_data = kmsg;

	kmsg->seq = log_first_seq;
	kmsg->ent = read_once(&tail);

	return 0;
}

static int kmsg_close(struct file *file)
{
	struct kmsg_output *kmsg = file->f_data;
	if (!kmsg)
		return -EBADF;

	file->f_data = NULL;
	free(kmsg);
	return 0;
}

static ssize_t
kmsg_read(struct file *file, void *buf, size_t len, off_t offset)
{
	struct kmsg_output *kmsg = file->f_data;
	if (!kmsg)
		return -EBADF;

	 /*
	  * REVISIT: the u_access* mechanism is broken for iov, so if
	  * _any_ usage of u_access_suspend() it must be unconditional
	  * so u_access_resume() can guarantee pointers are still
	  * valid
	  */
	bool ua = u_access_suspend();
	int rc;
	if ((log_last_seq - kmsg->seq) >= 0) /* seq can rollover */
		rc = 0;
	else if (file->f_flags & O_NONBLOCK)
		rc = -EAGAIN;
	else
		rc = wait_event_interruptible(log_wait, (log_last_seq - kmsg->seq) >= 0);

	int r = u_access_resume(ua, buf, len, PROT_WRITE);
	if (r < 0)
		return r;
	if (rc < 0)
		return rc;

	return kmsg_format(buf, len, kmsg);
}

static ssize_t
kmsg_read_iov(struct file *file, const struct iovec *iov, size_t count,
    off_t offset)
{
	return for_each_iov(file, iov, count, offset, kmsg_read);
}

/*
 * receive data from syslogd.
 */
static ssize_t
kmsg_write_iov(struct file *file, const struct iovec *iov, size_t count,
    off_t offset)
{
	ssize_t msg_len = 0;
	const struct iovec *p = iov;
	for (size_t i = count; i > 0; --i)
		msg_len += p++->iov_len;
	if (msg_len <= 0) {
		dbg("bad msg_len %d", msg_len);
		return msg_len;
	}

	int priority = LOG_USER | LOG_INFO;
	int pri_len = 0;
	const char *src = iov->iov_base;
	size_t iov_len = iov->iov_len;
	/* REVISIT: assumes complete log level in first iov */
	if (src[0] == '<') {
		char *p = NULL;
		unsigned int u;

		u = strtoul(src + 1, &p, 10);
		if (p && p[0] == '>') {
			/*
			 * LOG_FAC() & LOG_MAKEPRI() macros operate on
			 * facility without the shift applied:
			 *
			 * #define LOG_USER (1<<3)
			 * however LOG_FAC(LOG_USER) = 1
			 *
			 * if facility unset (0 == LOG_KERN) force LOG_USER
			 */
			int fac = (LOG_FACMASK & u) ? (LOG_FACMASK & u) : LOG_USER;
			priority = fac | LOG_PRI(u);
			p++;
			pri_len = p - src;
			iov_len -= pri_len;
			src = p;
		}
	}

	struct ent *entry;
	long seq;
	ssize_t len_term = syslog_begin(&entry, &seq, msg_len - pri_len);
	if (len_term < 0)
		return len_term;

	char *dst = entry->msg;
	if (iov_len) {
		memcpy(dst, src, iov_len);
		dst += iov_len;
	}
	while (--count) {
		++iov;
		memcpy(dst, iov->iov_base, iov->iov_len);
		dst += iov->iov_len;
	}
	entry->msg[msg_len] = '\0'; /* guarantee NULL terminated */

	syslog_end(entry, seq, priority);
	return msg_len;
}

static int
kmsg_seek(struct file *file, off_t offset, int whence)
{
	struct kmsg_output *kmsg = file->f_data;
	if (!kmsg)
		return -EBADF;

	if (offset)
		return -ESPIPE;

	switch (whence) {
	case SEEK_SET:
		kmsg->seq = log_first_seq;
		kmsg->ent = read_once(&tail);
		return 0;
	case SEEK_DATA:
		kmsg->seq = clear_seq;
		kmsg->ent = clear_ent;
		return 0;
	case SEEK_END:
		clear_seq = log_last_seq + 1;
		clear_ent = read_once(&head);
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * Device I/O table
 */
static struct devio kmsg_io = {
	.open = kmsg_open,
	.close = kmsg_close,
	.read = kmsg_read_iov,
	.write = kmsg_write_iov,
	.seek = kmsg_seek,
};

/*
 * Initialize
 */
void
kmsg_init(void)
{
	event_init(&log_wait, "kmsg_wait", ev_IO);

	/* Create device object */
	struct device *d = device_create(&kmsg_io, "kmsg", DF_CHR, NULL);
	assert(d);
}
