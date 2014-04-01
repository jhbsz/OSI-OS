#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/mtio.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include "dd.h"
#include "extern.h"

void pos_in(void) {
	off_t cnt;
	int warned;
	ssize_t nr;
	size_t bcnt;

	if (in.flags & ISSEEK) {
		errno = 0;
		if (lseek(in.fd, in.offset * in.dbsz, SEEK_CUR) == -1 && errno != 0)
			err(1, "%s", in.name);
		return;
	}
	if (in.offset < 0)
		errx(1, "%s: illegal offset", "iseek/skip");
	for (bcnt = in.dbsz, cnt = in.offset, warned = 0; cnt;) {
		if ((nr = read(in.fd, in.db, bcnt)) > 0) {
			if (in.flags & ISPIPE)
				if (!(bcnt -= nr)) {
					bcnt = in.dbsz;
					--cnt;
				}
			else
				--cnt;
			if (need_summary)
				summary();
			continue;
		}
		if (nr == 0) {
			if (files_cnt > 1) {
				--files_cnt;
				continue;
			}
			errx(1, "skip reached end of input");
		}
		if (ddflags & C_NOERROR) {
			if (!warned) {
				warn("%s", in.name);
				warned = 1;
				summary();
			}
			continue;
		}
		err(1, "%s", in.name);
	}
}

void pos_out(void) {
	struct mtop t_op;
	off_t cnt;
	ssize_t n;

	if (out.flags & (ISSEEK | ISPIPE)) {
		errno = 0;
		if (lseek(out.fd, out.offset * out.dbsz, SEEK_CUR) == -1 && errno != 0)
			err(1, "%s", out.name);
		return;
	}
	if (out.offset < 0)
		errx(1, "%s: illegal offset", "oseek/seek");
	if (out.flags & NOREAD) {
		t_op.mt_op = MTFSR;
		t_op.mt_count = out.offset;
		if (ioctl(out.fd, MTIOCTOP, &t_op) == -1)
			err(1, "%s", out.name);
		return;
	}
	for (cnt = 0; cnt < out.offset; ++cnt) {
		if ((n = read(out.fd, out.db, out.dbsz)) > 0)
			continue;
		if (n == -1)
			err(1, "%s", out.name);
		t_op.mt_op = MTBSR;
		t_op.mt_count = 1;
		if (ioctl(out.fd, MTIOCTOP, &t_op) == -1)
			err(1, "%s", out.name);
		while (cnt++ < out.offset) {
			n = write(out.fd, out.db, out.dbsz);
			if (n == -1)
				err(1, "%s", out.name);
			if ((size_t)n != out.dbsz)
				errx(1, "%s: write failure", out.name);
		}
		break;
	}
}
