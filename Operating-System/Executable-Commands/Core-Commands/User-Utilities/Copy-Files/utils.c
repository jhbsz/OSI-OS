#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
#include <sys/mman.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include "extern.h"

#define	cp_pct(x, y)	((y == 0) ? 0 : (int)(100.0 * (x) / (y)))
#define PHYSPAGES_THRESHOLD (32*1024)
#define BUFSIZE_MAX (2*1024*1024)
#define BUFSIZE_SMALL (MAXPHYS)

int copy_file(const FTSENT *entp, int dne) {
	static char *buf = NULL;
	static size_t bufsize;
	struct stat *fs;
	ssize_t wcount;
	size_t wresid;
	off_t wtotal;
	int ch, checkch, from_fd = 0, rcount, rval, to_fd = 0;
	char *bufp;
	#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
		char *p;
	#endif

	if ((from_fd = open(entp->fts_path, O_RDONLY, 0)) == -1) {
		warn("%s", entp->fts_path);
		return (1);
	}
	fs = entp->fts_statp;
	if (!dne) {
		#define YESNO "(y/n [n]) "
		if (nflag) {
			if (vflag)
				printf("%s not overwritten\n", to.p_path);
			(void)close(from_fd);
			return (1);
		} else if (iflag) {
			(void)fprintf(stderr, "overwrite %s? %s", to.p_path, YESNO);
			checkch = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (checkch != 'y' && checkch != 'Y') {
				(void)close(from_fd);
				(void)fprintf(stderr, "not overwritten\n");
				return (1);
			}
		}
		if (fflag) {
			(void)unlink(to.p_path);
			if (!lflag)
			to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT, fs->st_mode & ~(S_ISUID | S_ISGID));
		} else {
			if (!lflag)
				to_fd = open(to.p_path, O_WRONLY | O_TRUNC, 0);
		}
	} else {
		if (!lflag)
			to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT, fs->st_mode & ~(S_ISUID | S_ISGID));
	}
	if (to_fd == -1) {
		warn("%s", to.p_path);
		(void)close(from_fd);
		return (1);
	}
	rval = 0;
	if (!lflag) {
		#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
			if (S_ISREG(fs->st_mode) && fs->st_size > 0 && fs->st_size <= 8 * 1024 * 1024 &&
(p = mmap(NULL, (size_t)fs->st_size, PROT_READ, MAP_SHARED, from_fd, (off_t)0)) != MAP_FAILED) {
				wtotal = 0;
				for (bufp = p, wresid = fs->st_size; ; bufp += wcount, wresid -= (size_t)wcount) {
					wcount = write(to_fd, bufp, wresid);
					if (wcount <= 0)
						break;
					wtotal += wcount;
					if (info) {
						info = 0;
						(void)fprintf(stderr, "%s -> %s %3d%%\n", entp->fts_path, to.p_path, cp_pct(wtotal, fs->st_size));
					}
					if (wcount >= (ssize_t)wresid)
						break;
				}
				if (wcount != (ssize_t)wresid) {
					warn("%s", to.p_path);
					rval = 1;
				}
				if (munmap(p, fs->st_size) < 0) {
					warn("%s", entp->fts_path);
					rval = 1;
				}
			} else
		#endif
		{
			if (buf == NULL) {
				if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
					bufsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
				else
					bufsize = BUFSIZE_SMALL;
				buf = malloc(bufsize);
				if (buf == NULL)
					err(1, "Not enough memory");
			}
			wtotal = 0;
			while ((rcount = read(from_fd, buf, bufsize)) > 0) {
				for (bufp = buf, wresid = rcount; ; 	bufp += wcount, wresid -= wcount) {
					wcount = write(to_fd, bufp, wresid);
					if (wcount <= 0)
						break;
					wtotal += wcount;
					if (info) {
						info = 0;
						(void)fprintf(stderr, "%s -> %s %3d%%\n", entp->fts_path, to.p_path, cp_pct(wtotal, fs->st_size));
					}
					if (wcount >= (ssize_t)wresid)
						break;
				}
				if (wcount != (ssize_t)wresid) {
					warn("%s", to.p_path);
					rval = 1;
					break;
				}
			}
			if (rcount < 0) {
				warn("%s", entp->fts_path);
				rval = 1;
			}
		}
	} else {
		if (link(entp->fts_path, to.p_path)) {
			warn("%s", to.p_path);
			rval = 1;
		}
	}
	if (!lflag) {
		if (pflag && setfile(fs, to_fd))
			rval = 1;
		if (pflag && preserve_fd_acls(from_fd, to_fd) != 0)
			rval = 1;
		if (close(to_fd)) {
			warn("%s", to.p_path);
			rval = 1;
		}
	}
	(void)close(from_fd);
	return (rval);
}

int copy_link(const FTSENT *p, int exists) {
	int len;
	char llink[PATH_MAX];

	if (exists && nflag) {
		if (vflag)
			printf("%s not overwritten\n", to.p_path);
		return (1);
	}
	if ((len = readlink(p->fts_path, llink, sizeof(llink) - 1)) == -1) {
		warn("readlink: %s", p->fts_path);
		return (1);
	}
	llink[len] = '\0';
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (symlink(llink, to.p_path)) {
		warn("symlink: %s", llink);
		return (1);
	}
	return (pflag ? setfile(p->fts_statp, -1) : 0);
}

int copy_fifo(struct stat *from_stat, int exists) {
	if (exists && nflag) {
		if (vflag)
			printf("%s not overwritten\n", to.p_path);
		return (1);
	}
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mkfifo(to.p_path, from_stat->st_mode)) {
		warn("mkfifo: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

int copy_special(struct stat *from_stat, int exists) {
	if (exists && nflag) {
		if (vflag)
			printf("%s not overwritten\n", to.p_path);
		return (1);
	}
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mknod(to.p_path, from_stat->st_mode, from_stat->st_rdev)) {
		warn("mknod: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

int setfile(struct stat *fs, int fd) {
	static struct timeval tv[2];
	struct stat ts;
	int rval, gotstat, islink, fdval;

	rval = 0;
	fdval = fd != -1;
	islink = !fdval && S_ISLNK(fs->st_mode);
	fs->st_mode &= S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
	TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atim);
	TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtim);
	if (islink ? lutimes(to.p_path, tv) : utimes(to.p_path, tv)) {
		warn("%sutimes: %s", islink ? "l" : "", to.p_path);
		rval = 1;
	}
	if (fdval ? fstat(fd, &ts) : (islink ? lstat(to.p_path, &ts) : stat(to.p_path, &ts)))
		gotstat = 0;
	else {
		gotstat = 1;
		ts.st_mode &= S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
	}
	if (!gotstat || fs->st_uid != ts.st_uid || fs->st_gid != ts.st_gid)
		if (fdval ? fchown(fd, fs->st_uid, fs->st_gid) : (islink ? lchown(to.p_path, fs->st_uid, fs->st_gid) :
chown(to.p_path, fs->st_uid, fs->st_gid))) {
			if (errno != EPERM) {
				warn("chown: %s", to.p_path);
				rval = 1;
			}
			fs->st_mode &= ~(S_ISUID | S_ISGID);
		}
	if (!gotstat || fs->st_mode != ts.st_mode)
		if (fdval ? fchmod(fd, fs->st_mode) : (islink ? lchmod(to.p_path, fs->st_mode) :
chmod(to.p_path, fs->st_mode))) {
			warn("chmod: %s", to.p_path);
			rval = 1;
		}
	if (!gotstat || fs->st_flags != ts.st_flags)
		if (fdval ? fchflags(fd, fs->st_flags) : (islink ? lchflags(to.p_path, fs->st_flags) :
chflags(to.p_path, fs->st_flags))) {
			warn("chflags: %s", to.p_path);
			rval = 1;
		}
	return (rval);
}

int preserve_fd_acls(int source_fd, int dest_fd) {
	acl_t acl;
	acl_type_t acl_type;
	int acl_supported = 0, ret, trivial;

	ret = fpathconf(source_fd, _PC_ACL_NFS4);
	if (ret > 0 ) {
		acl_supported = 1;
		acl_type = ACL_TYPE_NFS4;
	} else if (ret < 0 && errno != EINVAL) {
		warn("fpathconf(..., _PC_ACL_NFS4) failed for %s", to.p_path);
		return (1);
	}
	if (acl_supported == 0) {
		ret = fpathconf(source_fd, _PC_ACL_EXTENDED);
		if (ret > 0 ) {
			acl_supported = 1;
			acl_type = ACL_TYPE_ACCESS;
		} else if (ret < 0 && errno != EINVAL) {
			warn("fpathconf(..., _PC_ACL_EXTENDED) failed for %s", to.p_path);
			return (1);
		}
	}
	if (acl_supported == 0)
		return (0);
	acl = acl_get_fd_np(source_fd, acl_type);
	if (acl == NULL) {
		warn("failed to get acl entries while setting %s", to.p_path);
		return (1);
	}
	if (acl_is_trivial_np(acl, &trivial)) {
		warn("acl_is_trivial() failed for %s", to.p_path);
		acl_free(acl);
		return (1);
	}
	if (trivial) {
		acl_free(acl);
		return (0);
	}
	if (acl_set_fd_np(dest_fd, acl, acl_type) < 0) {
		warn("failed to set acl entries for %s", to.p_path);
		acl_free(acl);
		return (1);
	}
	acl_free(acl);
	return (0);
}

int preserve_dir_acls(struct stat *fs, char *source_dir, char *dest_dir) {
	acl_t (*aclgetf)(const char *, acl_type_t);
	int (*aclsetf)(const char *, acl_type_t, acl_t);
	struct acl *aclp;
	acl_t acl;
	acl_type_t acl_type;
	int acl_supported = 0, ret, trivial;

	ret = pathconf(source_dir, _PC_ACL_NFS4);
	if (ret > 0) {
		acl_supported = 1;
		acl_type = ACL_TYPE_NFS4;
	} else if (ret < 0 && errno != EINVAL) {
		warn("fpathconf(..., _PC_ACL_NFS4) failed for %s", source_dir);
		return (1);
	}
	if (acl_supported == 0) {
		ret = pathconf(source_dir, _PC_ACL_EXTENDED);
		if (ret > 0) {
			acl_supported = 1;
			acl_type = ACL_TYPE_ACCESS;
		} else if (ret < 0 && errno != EINVAL) {
			warn("fpathconf(..., _PC_ACL_EXTENDED) failed for %s", source_dir);
			return (1);
		}
	}
	if (acl_supported == 0)
		return (0);
	if (S_ISLNK(fs->st_mode)) {
		aclgetf = acl_get_link_np;
		aclsetf = acl_set_link_np;
	} else {
		aclgetf = acl_get_file;
		aclsetf = acl_set_file;
	}
	if (acl_type == ACL_TYPE_ACCESS) {
		acl = aclgetf(source_dir, ACL_TYPE_DEFAULT);
		if (acl == NULL) {
			warn("failed to get default acl entries on %s", source_dir);
			return (1);
		}
		aclp = &acl->ats_acl;
		if (aclp->acl_cnt != 0 && aclsetf(dest_dir, ACL_TYPE_DEFAULT, acl) < 0) {
			warn("failed to set default acl entries on %s", dest_dir);
			acl_free(acl);
			return (1);
		}
		acl_free(acl);
	}
	acl = aclgetf(source_dir, acl_type);
	if (acl == NULL) {
		warn("failed to get acl entries on %s", source_dir);
		return (1);
	}
	if (acl_is_trivial_np(acl, &trivial)) {
		warn("acl_is_trivial() failed on %s", source_dir);
		acl_free(acl);
		return (1);
	}
	if (trivial) {
		acl_free(acl);
		return (0);
	}
	if (aclsetf(dest_dir, acl_type, acl) < 0) {
		warn("failed to set acl entries on %s", dest_dir);
		acl_free(acl);
		return (1);
	}
	acl_free(acl);
	return (0);
}

void usage(void) {
	(void)fprintf(stderr, "%s\n%s\n", "usage: cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpvx] source_file target_file",
"       cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpvx] source_file ...\ntarget_directory");
	exit(EX_USAGE);
}
