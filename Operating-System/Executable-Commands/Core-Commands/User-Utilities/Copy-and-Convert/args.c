#include <sys/cdefs.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "dd.h"
#include "extern.h"

static int	c_arg(const void *, const void *);
static int	c_conv(const void *, const void *);
static char *oper;
static void	f_bs(char *);
static void	f_cbs(char *);
static void	f_conv(char *);
static void	f_count(char *);
static void	f_files(char *);
static void	f_fillchar(char *);
static void	f_ibs(char *);
static void	f_if(char *);
static void	f_obs(char *);
static void	f_of(char *);
static void	f_seek(char *);
static void	f_skip(char *);
static uintmax_t get_num(const char *);
static off_t	get_off_t(const char *);

static const struct arg {
	const char *name;
	void (*f)(char *);
	u_int set, noset;
} args[] = {
	{ "bs",		f_bs,		C_BS,	 C_BS|C_IBS|C_OBS|C_OSYNC },
	{ "cbs",	f_cbs,		C_CBS,	 C_CBS },
	{ "conv",	f_conv,		0,	 0 },
	{ "count",	f_count,	C_COUNT, C_COUNT },
	{ "files",	f_files,	C_FILES, C_FILES },
	{ "fillchar",	f_fillchar,	C_FILL,	 C_FILL },
	{ "ibs",	f_ibs,		C_IBS,	 C_BS|C_IBS },
	{ "if",		f_if,		C_IF,	 C_IF },
	{ "iseek",	f_skip,		C_SKIP,	 C_SKIP },
	{ "obs",	f_obs,		C_OBS,	 C_BS|C_OBS },
	{ "of",		f_of,		C_OF,	 C_OF },
	{ "oseek",	f_seek,		C_SEEK,	 C_SEEK },
	{ "seek",	f_seek,		C_SEEK,	 C_SEEK },
	{ "skip",	f_skip,		C_SKIP,	 C_SKIP },
};

void jcl(char **argv) {
	struct arg *ap, tmp;
	char *arg;

	in.dbsz = out.dbsz = 512;
	while ((oper = *++argv) != NULL) {
		if ((oper = strdup(oper)) == NULL)
			errx(1, "unable to allocate space for the argument \"%s\"", *argv);
		if ((arg = strchr(oper, '=')) == NULL)
			errx(1, "unknown operand %s", oper);
		*arg++ = '\0';
		if (!*arg)
			errx(1, "no value specified for %s", oper);
		tmp.name = oper;
		if (!(ap = (struct arg *)bsearch(&tmp, args,
		    sizeof(args)/sizeof(struct arg), sizeof(struct arg), c_arg)))
			errx(1, "unknown operand %s", tmp.name);
		if (ddflags & ap->noset)
			errx(1, "%s: illegal argument combination or already set", tmp.name);
		ddflags |= ap->set;
		ap->f(arg);
	}
	if (ddflags & C_BS) {
		if (ddflags & (C_BLOCK | C_LCASE | C_SWAB | C_UCASE | C_UNBLOCK))
			ddflags &= ~C_BS;
		if (ddflags & C_BS && ddflags & (C_IBS | C_OBS))
			warnx("bs supersedes ibs and obs");
	}
	if (ddflags & (C_BLOCK | C_UNBLOCK)) {
		if (!(ddflags & C_CBS))
			errx(1, "record operations require cbs");
		if (cbsz == 0)
			errx(1, "cbs cannot be zero");
		cfunc = ddflags & C_BLOCK ? block : unblock;
	} else if (ddflags & C_CBS) {
		if (ddflags & (C_ASCII | C_EBCDIC)) {
			if (ddflags & C_ASCII) {
				ddflags |= C_UNBLOCK;
				cfunc = unblock;
			} else {
				ddflags |= C_BLOCK;
				cfunc = block;
			}
		} else
			errx(1, "cbs meaningless if not doing record operations");
	} else
		cfunc = def;
	if (in.offset > OFF_MAX / (ssize_t)in.dbsz || out.offset > OFF_MAX / (ssize_t)out.dbsz)
		errx(1, "seek offsets cannot be larger than %jd", (intmax_t)OFF_MAX);
}

static int c_arg(const void *a, const void *b) {
	return (strcmp(((const struct arg *)a)->name, ((const struct arg *)b)->name));
}

static void f_bs(char *arg) {
	uintmax_t res;

	res = get_num(arg);
	if (res < 1 || res > SSIZE_MAX)
		errx(1, "bs must be between 1 and %jd", (intmax_t)SSIZE_MAX);
	in.dbsz = out.dbsz = (size_t)res;
}

static void f_cbs(char *arg) {
	uintmax_t res;

	res = get_num(arg);
	if (res < 1 || res > SSIZE_MAX)
		errx(1, "cbs must be between 1 and %jd", (intmax_t)SSIZE_MAX);
	cbsz = (size_t)res;
}

static void f_count(char *arg) {
	intmax_t res;

	res = (intmax_t)get_num(arg);
	if (res < 0)
		errx(1, "count cannot be negative");
	if (res == 0)
		cpy_cnt = (uintmax_t)-1;
	else
		cpy_cnt = (uintmax_t)res;
}

static void f_files(char *arg) {
	files_cnt = get_num(arg);
	if (files_cnt < 1)
		errx(1, "files must be between 1 and %jd", (uintmax_t)-1);
}

static void f_fillchar(char *arg) {
	if (strlen(arg) != 1)
		errx(1, "need exactly one fill char");
	fill_char = arg[0];
}

static void f_ibs(char *arg) {
	uintmax_t res;
	
	if (!(ddflags & C_BS)) {
		res = get_num(arg);
		if (res < 1 || res > SSIZE_MAX)
			errx(1, "ibs must be between 1 and %jd", (intmax_t)SSIZE_MAX);
		in.dbsz = (size_t)res;
	}
}

static void f_if(char *arg) {
	in.name = arg;
}

static void f_obs(char *arg) {
	uintmax_t res;
	
	if (!(ddflags & C_BS)) {
		res = get_num(arg);
		if (res < 1 || res > SSIZE_MAX)
			errx(1, "obs must be between 1 and %jd", (intmax_t)SSIZE_MAX);
		out.dbsz = (size_t)res;
	}
}

static void f_of(char *arg) {
	out.name = arg;
}

static void f_seek(char *arg) {
	out.offset = get_off_t(arg);
}

static void f_skip(char *arg) {
	in.offset = get_off_t(arg);
}

static const struct conv {
	const char *name;
	u_int set, noset;
	const u_char *ctab;
} clist[] = {
	{ "ascii",	C_ASCII,	C_EBCDIC,	e2a_POSIX },
	{ "block",	C_BLOCK,	C_UNBLOCK,	NULL },
	{ "ebcdic",	C_EBCDIC,	C_ASCII,	a2e_POSIX },
	{ "ibm",	C_EBCDIC,	C_ASCII,	a2ibm_POSIX },
	{ "lcase",	C_LCASE,	C_UCASE,	NULL },
	{ "noerror",	C_NOERROR,	0,		NULL },
	{ "notrunc",	C_NOTRUNC,	0,		NULL },
	{ "oldascii",	C_ASCII,	C_EBCDIC,	e2a_32V },
	{ "oldebcdic",	C_EBCDIC,	C_ASCII,	a2e_32V },
	{ "oldibm",	C_EBCDIC,	C_ASCII,	a2ibm_32V },
	{ "osync",	C_OSYNC,	C_BS,		NULL },
	{ "pareven",	C_PAREVEN,	C_PARODD|C_PARSET|C_PARNONE, NULL},
	{ "parnone",	C_PARNONE,	C_PARODD|C_PARSET|C_PAREVEN, NULL},
	{ "parodd",	C_PARODD,	C_PAREVEN|C_PARSET|C_PARNONE, NULL},
	{ "parset",	C_PARSET,	C_PARODD|C_PAREVEN|C_PARNONE, NULL},
	{ "sparse",	C_SPARSE,	0,		NULL },
	{ "swab",	C_SWAB,		0,		NULL },
	{ "sync",	C_SYNC,		0,		NULL },
	{ "ucase",	C_UCASE,	C_LCASE,	NULL },
	{ "unblock",	C_UNBLOCK,	C_BLOCK,	NULL },
};

static void f_conv(char *arg) {
	struct conv *cp, tmp;

	while (arg != NULL) {
		tmp.name = strsep(&arg, ",");
		cp = bsearch(&tmp, clist, sizeof(clist) / sizeof(struct conv), sizeof(struct conv), c_conv);
		if (cp == NULL)
			errx(1, "unknown conversion %s", tmp.name);
		if (ddflags & cp->noset)
			errx(1, "%s: illegal conversion combination", tmp.name);
		ddflags |= cp->set;
		if (cp->ctab)
			ctab = cp->ctab;
	}
}

static int c_conv(const void *a, const void *b) {
	return (strcmp(((const struct conv *)a)->name, ((const struct conv *)b)->name));
}

static uintmax_t get_num(const char *val) {
	uintmax_t num, mult, prevnum;
	char *expr;

	errno = 0;
	num = strtouq(val, &expr, 0);
	if (errno != 0)
		err(1, "%s", oper);
	if (expr == val)
		errx(1, "%s: illegal numeric value", oper);
	mult = 0;
	switch (*expr) {
		case 'B':
		case 'b':
			mult = 512;
			break;
		case 'K':
		case 'k':
			mult = 1 << 10;
			break;
		case 'M':
		case 'm':
			mult = 1 << 20;
			break;
		case 'G':
		case 'g':
			mult = 1 << 30;
			break;
		case 'W':
		case 'w':
			mult = sizeof(int);
			break;
		default:
			;
	}
	if (mult != 0) {
		prevnum = num;
		num *= mult;
		if (num / mult != prevnum)
			goto erange;
		expr++;
	}
	switch (*expr) {
		case '\0':
			break;
		case '*':
		case 'X':
		case 'x':
			mult = get_num(expr + 1);
			prevnum = num;
			num *= mult;
			if (num / mult == prevnum)
				break;
erange:
			errx(1, "%s: %s", oper, strerror(ERANGE));
		default:
			errx(1, "%s: illegal numeric value", oper);
	}
	return (num);
}

static off_t get_off_t(const char *val) {
	intmax_t num, mult, prevnum;
	char *expr;

	errno = 0;
	num = strtoq(val, &expr, 0);
	if (errno != 0)
		err(1, "%s", oper);
	if (expr == val)
		errx(1, "%s: illegal numeric value", oper);
	mult = 0;
	switch (*expr) {
		case 'B':
		case 'b':
			mult = 512;
			break;
		case 'K':
		case 'k':
			mult = 1 << 10;
			break;
		case 'M':
		case 'm':
			mult = 1 << 20;
			break;
		case 'G':
		case 'g':
			mult = 1 << 30;
			break;
		case 'W':
		case 'w':
			mult = sizeof(int);
			break;
		}
	if (mult != 0) {
		prevnum = num;
		num *= mult;
		if ((prevnum > 0) != (num > 0) || num / mult != prevnum)
			goto erange;
		expr++;
	}
	switch (*expr) {
		case '\0':
			break;
		case '*':
		case 'X':
		case 'x':
			mult = (intmax_t)get_off_t(expr + 1);
			prevnum = num;
			num *= mult;
			if ((prevnum > 0) == (num > 0) && num / mult == prevnum)
				break;
erange:
			errx(1, "%s: %s", oper, strerror(ERANGE));
		default:
			errx(1, "%s: illegal numeric value", oper);
	}
	return (num);
}
