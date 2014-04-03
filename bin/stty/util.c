#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "stty.h"
#include "extern.h"

void checkredirect(void) {
	struct stat sb1, sb2;

	if (isatty(STDOUT_FILENO) && isatty(STDERR_FILENO) && !fstat(STDOUT_FILENO, &sb1)
&& !fstat(STDERR_FILENO, &sb2) && (sb1.st_rdev != sb2.st_rdev))
		warnx("stdout appears redirected, but stdin is the control descriptor");
}
