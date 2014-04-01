#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dd.h"
#include "extern.h"

void summary(void) {
	struct timeval tv;
	double secs;

	(void)gettimeofday(&tv, NULL);
	secs = tv.tv_sec + tv.tv_usec * 1e-6 - st.start;
	if (secs < 1e-6)
		secs = 1e-6;
	(void)fprintf(stderr, "%ju+%ju records in\n%ju+%ju records out\n",st.in_full, st.in_part, st.out_full, st.out_part);
	if (st.swab)
		(void)fprintf(stderr, "%ju odd length swab %s\n", st.swab, (st.swab == 1) ? "block" : "blocks");
	if (st.trunc)
		(void)fprintf(stderr, "%ju truncated %s\n", st.trunc, (st.trunc == 1) ? "block" : "blocks");
	(void)fprintf(stderr, "%ju bytes transferred in %.6f secs (%.0f bytes/sec)\n", st.bytes, secs, st.bytes / secs);
	need_summary = 0;
}

void siginfo_handler(int signo __unused) {
	need_summary = 1;
}

void terminate(int sig) {
	summary();
	_exit(sig == 0 ? 0 : 1);
}
