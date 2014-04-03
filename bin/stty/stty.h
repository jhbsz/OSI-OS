#include <sys/ioctl.h>
#include <termios.h>

struct info {
	int fd;
	int ldisc;
	int off;
	int set;
	int wset;
	const char *arg;
	struct termios t;
	struct winsize win;
};

struct cchar {
	const char *name;
	int sub;
	u_char def;
};

enum FMT { NOTSET, GFLAG, BSD, POSIX };

#define	LINELENGTH	72
