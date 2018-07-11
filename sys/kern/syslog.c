/*
 * syslog.c - system log
 */

#include <debug.h>

#include <access.h>
#include <arch.h>
#include <errno.h>
#include <irq.h>
#include <kernel.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <timer.h>

/*
 * ent - system log entry
 */
struct ent {
	uint64_t nsec;		/* timestamp in nanoseconds */
	uint64_t seq;		/* sequence number of message */
	size_t len;		/* length of msg including null terminator */
	uint8_t facility;	/* syslog facility */
	uint8_t level;		/* syslog level */
	char msg[];		/* message text */
};

static char log[CONFIG_SYSLOG_SIZE] __attribute__((aligned(alignof(struct ent))));
static uint64_t log_seq, output_seq;
static struct ent *head = (struct ent *)log;
static struct ent *tail = (struct ent *)log;

static int conlev = CONFIG_CONSOLE_LOGLEVEL + 1;
static const int min_conlev = LOG_WARNING + 1;

static void early_console_output(void);
static void (*log_output)(void) =
#if defined(CONFIG_EARLY_CONSOLE)
	early_console_output;
#else
	0;
#endif

static_assert((sizeof(log) & (sizeof(log) - 1)) == 0,
    "SYSLOG_SIZE must be a power of 2");

static_assert(sizeof(log) >= 128,
    "SYSLOG_SIZE must be at least 128 bytes");

/*
 * to_end - number of bytes remaining between head and end of log
 */
static size_t
to_end(void)
{
	return log + sizeof(log) - (char *)head;
}

/*
 * to_tail - number of bytes between head and tail
 */
static size_t
to_tail(void)
{
	if (tail == head)
		return sizeof(log);
	return ((char *)tail - (char *)head) & (sizeof(log) - 1);
}

/*
 * buf_linear - linear free space in log
 */
static size_t
buf_linear(void)
{
	return MIN(to_end(), to_tail());
}

/*
 * advance - move pointer to next entry
 */
static void
advance(struct ent **p)
{
	if ((*p)->len == SIZE_MAX) {
		*p = (struct ent *)log;
		return;
	}

	*p = (struct ent *)ALIGNn((char *)*p + sizeof(**p) + (*p)->len,
	    alignof(**p));
	if ((char *)(*p + 1) > (log + sizeof(log)))
		*p = (struct ent *)log;
}

/*
 * early_console_output - print any new messages to the console
 *
 * Must be interrupt save.
 */
static void
early_console_output(void)
{
	int len;
	char buf[256];
	int s = irq_disable();

	while ((len = syslog_format(buf, sizeof(buf))) > 0)
		early_console_print(buf, len);

	irq_restore(s);
}

/*
 * log_write - write a new entry to the log buffer
 */
static int
log_write(int facility, int level, const char *fmt, va_list ap)
{
	size_t max_msg = buf_linear() - sizeof(*head);
	int ret = vsnprintf(head->msg, max_msg, fmt, ap);

	/* error */
	if (ret < 0)
		return ret;

	/* too long */
	if (ret >= max_msg)
		return ret + sizeof(*head);

	head->nsec = timer_monotonic();
	head->seq = log_seq;
	head->len = ret;
	head->facility = facility;
	head->level = level;

	advance(&head);

	return 0;
}

/*
 * log_trim - trim log buffer until at least len bytes are free
 */
static void
log_trim(const size_t len)
{
	/* need to wrap back to start */
	if (len > to_end()) {
		head->len = SIZE_MAX;	/* special marker */
		advance(&head);
	}

	while (buf_linear() < len)
		advance(&tail);
}

/*
 * syslog_printf - print debug string to log & console
 *
 * Must be interrupt safe.
 *
 * No point locking the scheduler here as this function can be called from
 * interrupt handlers.
 */
void
syslog_printf(int level, const char *fmt, ...)
{
	const int s = irq_disable();

	va_list ap;
	va_start(ap, fmt);

	const int len = log_write(LOG_KERN, level, fmt, ap);
	if (len <= 0 || len > sizeof(log))
		goto out;   /* success or unrecoverable error */
	log_trim(len);
	log_write(LOG_KERN, level, fmt, ap);

out:
	va_end(ap);
	++log_seq;

	irq_restore(s);

	if (log_output)
		log_output();
}

/*
 * syslog_output - register function to be called when new messages available
 */
void
syslog_output(void (*fn)(void))
{
	log_output = fn;
}

/*
 * syslog_format - format log messages for console
 *
 * Must be interrupt safe.
 */
int
syslog_format(char *buf, const size_t len)
{
	int n, rem = len;
	int s = irq_disable();

	for (; tail != head; advance(&tail)) {
		if (tail->len == SIZE_MAX)
			continue;

		if (output_seq != tail->seq) {
			n = snprintf(buf, rem, "*** lost %llu messages\n",
			    tail->seq - output_seq);
			if (n < 0 || n >= rem)
				break;
			buf += n;
			rem -= n;
			output_seq = tail->seq;
		}

		if (tail->level < conlev) {
			n = snprintf(buf, rem, "[%5lu.%06llu] %s",
			    (unsigned long)(tail->nsec / 1000000000),
			    (tail->nsec % 1000000000) / 1000,
			    tail->msg);
			if (n < 0 || n >= len) {
				++output_seq;	    /* skip message, notify */
				break;
			}
			if (n >= rem)
				break;
			buf += n;
			rem -= n;
		}

		++output_seq;
	}

	irq_restore(s);

	return len - rem;
}

/*
 * syslog_panic - flush log after panic
 *
 * Called with all interrupts disabled.
 */
void
syslog_panic(void)
{
	early_console_init();

	early_console_print("\n*** syslog_panic\n", 18);

	tail = (struct ent *)log;
	output_seq = 0;
	early_console_output();
	log_output = early_console_output;
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
		/* TODO: implement */
		return DERR(-ENOSYS);
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


