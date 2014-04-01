typedef struct {
	char	*p_end;
	char	*target_end;
	char	p_path[PATH_MAX];
} PATH_T;

extern PATH_T to;
extern int fflag, iflag, lflag, nflag, pflag, vflag;
extern volatile sig_atomic_t info;

__BEGIN_DECLS
int	copy_fifo(struct stat *, int);
int	copy_file(const FTSENT *, int);
int	copy_link(const FTSENT *, int);
int	copy_special(struct stat *, int);
int	setfile(struct stat *, int);
int	preserve_dir_acls(struct stat *, char *, char *);
int	preserve_fd_acls(int, int);
void	usage(void);
__END_DECLS
