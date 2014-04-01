typedef struct {
	u_char		*db;
	u_char		*dbp;
	size_t		dbcnt;
	size_t		dbrcnt;
	size_t		dbsz;

#define	ISCHR		0x01
#define	ISPIPE		0x02
#define	ISTAPE		0x04
#define	ISSEEK		0x08
#define	NOREAD		0x10
#define	ISTRUNC		0x20

	u_int		flags;
	const char	*name;
	int		fd;
	off_t		offset;
} IO;

typedef struct {
	uintmax_t	in_full;
	uintmax_t	in_part;
	uintmax_t	out_full;
	uintmax_t	out_part;
	uintmax_t	trunc;
	uintmax_t	swab;
	uintmax_t	bytes;
	double		start;
} STAT;

#define	C_ASCII		0x00001
#define	C_BLOCK		0x00002
#define	C_BS		0x00004
#define	C_CBS		0x00008
#define	C_COUNT		0x00010
#define	C_EBCDIC	0x00020
#define	C_FILES		0x00040
#define	C_IBS		0x00080
#define	C_IF		0x00100
#define	C_LCASE		0x00200
#define	C_NOERROR	0x00400
#define	C_NOTRUNC	0x00800
#define	C_OBS		0x01000
#define	C_OF		0x02000
#define	C_OSYNC		0x04000
#define	C_PAREVEN	0x08000
#define	C_PARNONE	0x100000
#define	C_PARODD	0x200000
#define	C_PARSET	0x400000
#define	C_SEEK		0x800000
#define	C_SKIP		0x1000000
#define	C_SPARSE	0x2000000
#define	C_SWAB		0x4000000
#define	C_SYNC		0x8000000
#define	C_UCASE		0x10000000
#define	C_UNBLOCK	0x20000000
#define	C_FILL		0x40000000
#define	C_PARITY	(C_PAREVEN | C_PARODD | C_PARNONE | C_PARSET)
