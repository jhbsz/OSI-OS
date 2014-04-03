#include <sys/cdefs.h>
#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stty.h"
#include "extern.h"

int main(int argc, char *argv[]) {
	struct info i;
	enum FMT fmt;
	int ch;
	const char *file;

	fmt = NOTSET;
	i.fd = STDIN_FILENO;
	file = "stdin";
	opterr = 0;
	while (optind < argc && strspn(argv[optind], "-aefg") == strlen(argv[optind])
&& (ch = getopt(argc, argv, "aef:g")) != -1)
		switch(ch) {
			case 'a':
				fmt = POSIX;
				break;
			case 'e':
				fmt = BSD;
				break;
			case 'f':
				if ((i.fd = open(optarg, O_RDONLY | O_NONBLOCK)) < 0)
					err(1, "%s", optarg);
				file = optarg;
				break;
			case 'g':
				fmt = GFLAG;
				break;
			case '?':
			default:
				goto args;
		}
args:
	argc -= optind;
	argv += optind;
	if (tcgetattr(i.fd, &i.t) < 0)
		errx(1, "%s isn't a terminal", file);
	if (ioctl(i.fd, TIOCGETD, &i.ldisc) < 0)
		err(1, "TIOCGETD");
	if (ioctl(i.fd, TIOCGWINSZ, &i.win) < 0)
		warn("TIOCGWINSZ");
	checkredirect();
	switch(fmt) {
		case NOTSET:
			if (*argv)
				break;
		case BSD:
		case POSIX:
			print(&i.t, &i.win, i.ldisc, fmt);
			break;
		case GFLAG:
			gprint(&i.t, &i.win, i.ldisc);
	}
	for (i.set = i.wset = 0; *argv; ++argv) {
		if (ksearch(&argv, &i))
			continue;
		if (csearch(&argv, &i))
			continue;
		if (msearch(&argv, &i))
			continue;
		if (isdigit(**argv)) {
			speed_t speed;

			speed = atoi(*argv);
			cfsetospeed(&i.t, speed);
			cfsetispeed(&i.t, speed);
			i.set = 1;
			continue;
		}
		if (!strncmp(*argv, "gfmt1", sizeof("gfmt1") - 1)) {
			gread(&i.t, *argv + sizeof("gfmt1") - 1);
			i.set = 1;
			continue;
		}
		warnx("illegal option -- %s", *argv);
		usage();
	}
	if (i.set && tcsetattr(i.fd, 0, &i.t) < 0)
		err(1, "tcsetattr");
	if (i.wset && ioctl(i.fd, TIOCSWINSZ, &i.win) < 0)
		warn("TIOCSWINSZ");
	exit(0);
}

void usage(void) {
	(void)fprintf(stderr, "usage: stty [-a | -e | -g] [-f file] [arguments]\n");
	exit (1);
}
