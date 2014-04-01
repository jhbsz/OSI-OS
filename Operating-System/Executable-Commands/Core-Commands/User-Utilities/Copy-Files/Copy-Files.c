#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "extern.h"

#define	STRIP_TRAILING_SLASH(p) {					\
        while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')	\
                *--(p).p_end = 0;					\
}

static char emptystring[] = "";
int fflag, iflag, lflag, nflag, pflag, vflag;
static int Rflag, rflag;
volatile sig_atomic_t info;
enum op { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

PATH_T to = { to.p_path, emptystring, "" };

static int copy(char *[], enum op, int);
static int mastercmp(const FTSENT * const *, const FTSENT * const *);
static void siginfo(int __unused);

int main(int argc, char *argv[]) {
	struct stat to_stat, tmp_stat;
	enum op type;
	int Hflag, Lflag, ch, fts_options, r, have_trailing_slash;
	char *target;

	fts_options = FTS_NOCHDIR | FTS_PHYSICAL;
	Hflag = Lflag = 0;
	while ((ch = getopt(argc, argv, "HLPRafilnprvx")) != -1)
		switch (ch) {
			case 'H':
				Hflag = 1;
				Lflag = 0;
				break;
			case 'L':
				Lflag = 1;
				Hflag = 0;
				break;
			case 'P':
				Hflag = Lflag = 0;
				break;
			case 'R':
				Rflag = 1;
				break;
			case 'a':
				pflag = 1;
				Rflag = 1;
				Hflag = Lflag = 0;
				break;
			case 'f':
				fflag = 1;
				iflag = nflag = 0;
				break;
			case 'i':
				iflag = 1;
				fflag = nflag = 0;
				break;
			case 'l':
				lflag = 1;
				break;
			case 'n':
				nflag = 1;
				fflag = iflag = 0;
				break;
			case 'p':
				pflag = 1;
				break;
			case 'r':
				rflag = Lflag = 1;
				Hflag = 0;
				break;
			case 'v':
				vflag = 1;
				break;
			case 'x':
				fts_options |= FTS_XDEV;
				break;
			default:
				usage();
				break;
		}
	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();
	if (Rflag && rflag)
		errx(1, "the -R and -r options may not be specified together");
	if (rflag)
		Rflag = 1;
	if (Rflag) {
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else {
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL | FTS_COMFOLLOW;
	}
	(void)signal(SIGINFO, siginfo);
	target = argv[--argc];
	if (strlcpy(to.p_path, target, sizeof(to.p_path)) >= sizeof(to.p_path))
		errx(1, "%s: name too long", target);
	to.p_end = to.p_path + strlen(to.p_path);
	if (to.p_path == to.p_end) {
		*to.p_end++ = '.';
		*to.p_end = 0;
	}
	have_trailing_slash = (to.p_end[-1] == '/');
	if (have_trailing_slash)
		STRIP_TRAILING_SLASH(to);
	to.target_end = to.p_end;
	argv[argc] = NULL;
	r = stat(to.p_path, &to_stat);
	if (r == -1 && errno != ENOENT)
		err(1, "%s", to.p_path);
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		if (argc > 1)
			errx(1, "%s is not a directory", to.p_path);
		if (r == -1) {
			if (Rflag && (Lflag || Hflag))
				stat(*argv, &tmp_stat);
			else
				lstat(*argv, &tmp_stat);
			if (S_ISDIR(tmp_stat.st_mode) && Rflag)
				type = DIR_TO_DNE;
			else
				type = FILE_TO_FILE;
		} else
			type = FILE_TO_FILE;
		if (have_trailing_slash && type == FILE_TO_FILE) {
			if (r == -1)
				errx(1, "directory %s does not exist", to.p_path);
			else
				errx(1, "%s is not a directory", to.p_path);
		}
	} else
		type = FILE_TO_DIR;
	exit (copy(argv, type, fts_options));
}

static int copy(char *argv[], enum op type, int fts_options) {
	struct stat to_stat;
	FTS *ftsp;
	FTSENT *curr;
	int base = 0, dne, badcp, rval;
	size_t nlen;
	char *p, *target_mid;
	mode_t mask, mode;

	mask = ~umask(0777);
	umask(~mask);

	if ((ftsp = fts_open(argv, fts_options, mastercmp)) == NULL)
		err(1, "fts_open");
	for (badcp = rval = 0; (curr = fts_read(ftsp)) != NULL; badcp = 0) {
		switch (curr->fts_info) {
			case FTS_NS:
			case FTS_DNR:
			case FTS_ERR:
				warnx("%s: %s", curr->fts_path, strerror(curr->fts_errno));
				badcp = rval = 1;
				continue;
			case FTS_DC:
				warnx("%s: directory causes a cycle", curr->fts_path);
				badcp = rval = 1;
				continue;
			default:
		}
		if (type != FILE_TO_FILE) {
			if (curr->fts_level == FTS_ROOTLEVEL) {
				if (type != DIR_TO_DNE) {
					p = strrchr(curr->fts_path, '/');
					base = (p == NULL) ? 0 : (int)(p - curr->fts_path + 1);
					if (!strcmp(&curr->fts_path[base], ".."))
						base += 1;
				} else
					base = curr->fts_pathlen;
			}
			p = &curr->fts_path[base];
			nlen = curr->fts_pathlen - base;
			target_mid = to.target_end;
			if (*p != '/' && target_mid[-1] != '/')
				*target_mid++ = '/';
			*target_mid = 0;
			if (target_mid - to.p_path + nlen >= PATH_MAX) {
				warnx("%s%s: name too long (not copied)", to.p_path, p);
				badcp = rval = 1;
				continue;
			}
			(void)strncat(target_mid, p, nlen);
			to.p_end = target_mid + nlen;
			*to.p_end = 0;
			STRIP_TRAILING_SLASH(to);
		}
		if (curr->fts_info == FTS_DP) {
			if (!curr->fts_number)
				continue;
			if (pflag) {
				if (setfile(curr->fts_statp, -1))
					rval = 1;
				if (preserve_dir_acls(curr->fts_statp,
					curr->fts_accpath, to.p_path) != 0)
					rval = 1;
			} else {
				mode = curr->fts_statp->st_mode;
				if ((mode & (S_ISUID | S_ISGID | S_ISTXT)) || ((mode | S_IRWXU) & mask) != (mode & mask))
					if (chmod(to.p_path, mode & mask) != 0) {
						warn("chmod: %s", to.p_path);
						rval = 1;
					}
			}
			continue;
		}
		if (stat(to.p_path, &to_stat) == -1)
			dne = 1;
		else {
			if (to_stat.st_dev == curr->fts_statp->st_dev && to_stat.st_ino == curr->fts_statp->st_ino) {
				warnx("%s and %s are identical (not copied).", to.p_path, curr->fts_path);
				badcp = rval = 1;
				if (S_ISDIR(curr->fts_statp->st_mode))
					(void)fts_set(ftsp, curr, FTS_SKIP);
				continue;
			}
			if (!S_ISDIR(curr->fts_statp->st_mode) && S_ISDIR(to_stat.st_mode)) {
				warnx("cannot overwrite directory %s with non-directory %s", to.p_path, curr->fts_path);
				badcp = rval = 1;
				continue;
			}
			dne = 0;
		}
		switch (curr->fts_statp->st_mode & S_IFMT) {
			case S_IFLNK:
				if ((fts_options & FTS_LOGICAL) || ((fts_options & FTS_COMFOLLOW) && curr->fts_level == 0)) {
					if (copy_file(curr, dne))
						badcp = rval = 1;
				} else {	
					if (copy_link(curr, !dne))
						badcp = rval = 1;
				}
				break;
			case S_IFDIR:
				if (!Rflag) {
					warnx("%s is a directory (not copied).", curr->fts_path);
					(void)fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					break;
				}
				if (dne) {
					if (mkdir(to.p_path, curr->fts_statp->st_mode | S_IRWXU) < 0)
						err(1, "%s", to.p_path);
				} else if (!S_ISDIR(to_stat.st_mode)) {
					errno = ENOTDIR;
					err(1, "%s", to.p_path);
				}
				curr->fts_number = pflag || dne;
				break;
			case S_IFBLK:
			case S_IFCHR:
				if (Rflag) {
					if (copy_special(curr->fts_statp, !dne))
						badcp = rval = 1;
				} else {
					if (copy_file(curr, dne))
						badcp = rval = 1;
				}
				break;
			case S_IFSOCK:
				warnx("%s is a socket (not copied).", curr->fts_path);
				break;
			case S_IFIFO:
				if (Rflag) {
					if (copy_fifo(curr->fts_statp, !dne))
						badcp = rval = 1;
				} else {
					if (copy_file(curr, dne))
						badcp = rval = 1;
				}
				break;
			default:
				if (copy_file(curr, dne))
					badcp = rval = 1;
				break;
		}
		if (vflag && !badcp)
			(void)printf("%s -> %s\n", curr->fts_path, to.p_path);
	}
	if (errno)
		err(1, "fts_read");
	fts_close(ftsp);
	return (rval);
}

static int mastercmp(const FTSENT * const *a, const FTSENT * const *b) {
	int a_info, b_info;

	a_info = (*a)->fts_info;
	if (a_info == FTS_ERR || a_info == FTS_NS || a_info == FTS_DNR)
		return (0);
	b_info = (*b)->fts_info;
	if (b_info == FTS_ERR || b_info == FTS_NS || b_info == FTS_DNR)
		return (0);
	if (a_info == FTS_D)
		return (-1);
	if (b_info == FTS_D)
		return (1);
	return (0);
}

static void siginfo(int sig __unused) {
	info = 1;
}
