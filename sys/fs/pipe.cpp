#include "pipe.h"

#include "file.h"
#include "vnode.h"
#include <address.h>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <page.h>
#include <sig.h>
#include <sync.h>
#include <sys/stat.h>
#include <task.h>

/*
 * From opengroup.org...
 * When attempting to read from an empty pipe or FIFO:
 *
 * If no process has the pipe open for writing, read() will return 0
 * to indicate end-of-file.
 *
 * If some process has the pipe open for writing and O_NONBLOCK is
 * set, read() will return -1 and set errno to [EAGAIN].
 *
 * If some process has the pipe open for writing and O_NONBLOCK is
 * clear, read() will block the calling thread until some data is
 * written or the pipe is closed by all processes that had the pipe
 * open for writing.
 */

static_assert(PIPE_BUF == 4096, "");

#define pdbg(...)

/*
 * Page ownership identifier for pipe
 */
static char pipe_id;

/*
 * pipe_data
 */
struct pipe_data {
	struct cond  cond;	    /* condition variable for this pipe */
	size_t	     read_fds;	    /* number of fd open for reading */
	size_t	     write_fds;	    /* number of fd open for writing */
	size_t	     wr;	    /* write bytes */
	size_t	     rd;	    /* read bytes */
	std::byte *buf;	    /* pipe data buffer */
};

/*
 * pipe_alloc
 */
static int
pipe_alloc(file *fp)
{
	vnode *vp = fp->f_vnode;

	if (vp->v_pipe)
		return 0;

	phys *b;
	if (!(b = page_alloc(PIPE_BUF, MA_NORMAL, &pipe_id)))
		return -ENOMEM;

	pipe_data *p;
	if (!(p = (pipe_data *)malloc(sizeof *p))) {
		page_free(b, PIPE_BUF, &pipe_id);
		return -ENOMEM;
	}
	cond_init(&p->cond);
	p->read_fds = 0;
	p->write_fds = 0;
	p->wr = 0;
	p->rd = 0;
	p->buf = (std::byte *)phys_to_virt(b);

	vp->v_pipe = p;
	return 0;
}

/*
 * pipe_free
 */
static void
pipe_free(file *fp)
{
	vnode *vp = fp->f_vnode;
	pipe_data *p = (pipe_data *)vp->v_pipe;

	page_free(virt_to_phys(p->buf), PIPE_BUF, &pipe_id);
	free(p);
}

/*
 * pipe_open
 */
int
pipe_open(file *fp, int flags, mode_t mode)
{
	int ret;
	vnode *vp = fp->f_vnode;

	if (!S_ISFIFO(vp->v_mode))
		return DERR(-EINVAL);

	if ((ret = pipe_alloc(fp)) < 0)
		return ret;

	pipe_data *p = (pipe_data *)vp->v_pipe;

	if ((flags & (O_NONBLOCK | O_ACCMODE)) == (O_NONBLOCK | O_WRONLY)
	    && p->read_fds == 0)
		return -ENXIO;

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		p->read_fds++;
		break;
	case O_WRONLY:
		p->write_fds++;
		break;
	default:
		return -EINVAL;
	}
	return 0;

}

/*
 * pipe_close
 */
int
pipe_close(file *fp)
{
	vnode *vp = fp->f_vnode;

	if (!S_ISFIFO(vp->v_mode))
		return DERR(-EINVAL);

	pipe_data *p = (pipe_data *)vp->v_pipe;

	switch (fp->f_flags & O_ACCMODE) {
	case O_RDONLY:
		if (--p->read_fds == 0)
			cond_signal(&p->cond); /* wake blocked write */
		break;
	case O_WRONLY:
		if (--p->write_fds == 0)
			cond_signal(&p->cond); /* wake blocked read */
		break;
	}

	if (!p->read_fds && !p->write_fds)
		pipe_free(fp);

	return 0;
}

/*
 * pipe_read
 */
ssize_t
pipe_read(file *fp, void *buf, size_t size, off_t offset)
{
	int err = 0;
	size_t read = 0;	/* bytes read so far */
	vnode *vp = fp->f_vnode;

	if (!S_ISFIFO(vp->v_mode))
		return DERR(-EINVAL);

	pipe_data *p = (pipe_data *)vp->v_pipe;

	while (size != 0) {
		size_t avail = p->wr - p->rd;
		pdbg("read: %d, %d remaining\n", read, size);
		if (avail == 0) {
			if (p->write_fds == 0)
				break; /* no writers: EOF */
			if (read > 0)
				break; /* data read, return */
			if (fp->f_flags & O_NONBLOCK) {
				err = -EAGAIN;
				break;
			}

			/* wait for write or close */
			pdbg("read: no data, wait\n");
			err = cond_wait_interruptible(&p->cond, &vp->v_lock);
			if (err)
				break;
			continue; /* validate data available */
		} else if (avail == PIPE_BUF) {
			pdbg("read: full, signal\n");
			/* notify write: will have space when we unlock the mutex */
			cond_signal(&p->cond);
		}

		/* offset into circular buf */
		size_t off = p->rd & (PIPE_BUF - 1);

		/* contiguous data available to end of curcular buffer */
		if (avail > PIPE_BUF - off)
			avail = PIPE_BUF - off;

		size_t len = (size < avail) ? size : avail;
		pdbg("read: off %d len %d avail %d\n", off, len, avail);
		memcpy(buf, p->buf + off, len);
		p->rd += len;
		read += len;
		size -= len;
		buf = (char *)buf + len;
	}

	return (read > 0) ? (ssize_t)read : err;
}

/*
 * pipe_write
 */
ssize_t
pipe_write(file *fp, void *buf, size_t size, off_t offset)
{
	int err = 0;
	size_t written = 0;	/* bytes written so far */
	vnode *vp = fp->f_vnode;

	if (!S_ISFIFO(vp->v_mode))
		return DERR(-EINVAL);

	pipe_data *p = (pipe_data *)vp->v_pipe;

	while (size != 0) {
		if (p->read_fds == 0) {
			sig_task(task_cur(), SIGPIPE);
			err = -EPIPE;
			break;
		}
		size_t free = PIPE_BUF - (p->wr - p->rd);
		pdbg("written: %d, %d remaining\n", written, size);
		if (free == 0) {
			if (fp->f_flags & O_NONBLOCK) {
				err = -EAGAIN;
				break;
			}

			/* wait for read or close */
			pdbg("write: full, wait\n");
			err = cond_wait_interruptible(&p->cond, &vp->v_lock);
			if (err)
				break;
			continue; /* calculate free again */
		} else if (free == PIPE_BUF) {
			pdbg("write: empty, signal\n");
			/* notify read: will have data when we unlock the mutex */
			cond_signal(&p->cond);
		}

		/* offset into circular buf */
		size_t off = p->wr & (PIPE_BUF - 1);
		if (free > PIPE_BUF - off)
			free = PIPE_BUF - off; /* space wrapped in buffer */

		size_t len = (size < free) ? size : free;
		pdbg("write: off %d len %d free %d\n", off, len, free);
		memcpy(p->buf + off, buf, len);
		p->wr += len;
		written += len;
		size -= len;
		buf = (char *)buf + len;
	}

	return (written > 0) ? (ssize_t)written : err;
}
