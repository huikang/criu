#ifndef __CR_UTIL_H__
#define __CR_UTIL_H__

/*
 * Some bits are stolen from perf and kvm tools
 */
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <dirent.h>

#include "compiler.h"
#include "asm/types.h"
#include "xmalloc.h"
#include "bug.h"
#include "log.h"
#include "err.h"

#include "protobuf/vma.pb-c.h"

#define PREF_SHIFT_OP(pref, op, size)	((size) op (pref ##BYTES_SHIFT))
#define KBYTES_SHIFT	10
#define MBYTES_SHIFT	20
#define GBYTES_SHIFT	30

#define KBYTES(size)	PREF_SHIFT_OP(K, >>, size)
#define MBYTES(size)	PREF_SHIFT_OP(M, >>, size)
#define GBYTES(size)	PREF_SHIFT_OP(G, >>, size)

#define KILO(size)	PREF_SHIFT_OP(K, <<, size)
#define MEGA(size)	PREF_SHIFT_OP(M, <<, size)
#define GIGA(size)	PREF_SHIFT_OP(G, <<, size)

struct vma_area;
struct list_head;

extern void pr_vma(unsigned int loglevel, const struct vma_area *vma_area);

#define pr_info_vma(vma_area)	pr_vma(LOG_INFO, vma_area)
#define pr_msg_vma(vma_area)	pr_vma(LOG_MSG, vma_area)

#define pr_vma_list(level, head)				\
	do {							\
		struct vma_area *vma;				\
		list_for_each_entry(vma, head, list)		\
			pr_vma(level, vma);			\
	} while (0)
#define pr_info_vma_list(head)	pr_vma_list(LOG_INFO, head)

extern int move_img_fd(int *img_fd, int want_fd);
extern int close_safe(int *fd);

extern int reopen_fd_as_safe(char *file, int line, int new_fd, int old_fd, bool allow_reuse_fd);
#define reopen_fd_as(new_fd, old_fd)		reopen_fd_as_safe(__FILE__, __LINE__, new_fd, old_fd, false)
#define reopen_fd_as_nocheck(new_fd, old_fd)	reopen_fd_as_safe(__FILE__, __LINE__, new_fd, old_fd, true)

extern void close_proc(void);
extern int open_pid_proc(pid_t pid);
extern int close_pid_proc(void);
extern int set_proc_fd(int fd);

/*
 * Values for pid argument of the proc opening routines below.
 * SELF would open file under /proc/self
 * GEN would open a file under /proc itself
 * NONE is internal, don't use it ;)
 */

#define PROC_SELF	0
#define PROC_GEN	-1
#define PROC_NONE	-2

extern int do_open_proc(pid_t pid, int flags, const char *fmt, ...);

#define __open_proc(pid, flags, fmt, ...)				\
	({								\
		int __fd = do_open_proc(pid, flags,			\
					fmt, ##__VA_ARGS__);		\
		if (__fd < 0)						\
			pr_perror("Can't open %d/" fmt " on procfs",	\
					pid, ##__VA_ARGS__);		\
									\
		__fd;							\
	})

/* int open_proc(pid_t pid, const char *fmt, ...); */
#define open_proc(pid, fmt, ...)				\
	__open_proc(pid, O_RDONLY, fmt, ##__VA_ARGS__)

/* int open_proc_rw(pid_t pid, const char *fmt, ...); */
#define open_proc_rw(pid, fmt, ...)				\
	__open_proc(pid, O_RDWR, fmt, ##__VA_ARGS__)

/* DIR *opendir_proc(pid_t pid, const char *fmt, ...); */
#define opendir_proc(pid, fmt, ...)					\
	({								\
		int __fd = open_proc(pid, fmt, ##__VA_ARGS__);		\
		DIR *__d = NULL;					\
									\
		if (__fd >= 0) {					\
			__d = fdopendir(__fd);				\
			if (__d == NULL)				\
				pr_perror("Can't fdopendir %d "		\
					"(%d/" fmt " on procfs)",	\
					__fd, pid, ##__VA_ARGS__);	\
		}							\
		__d;							\
	 })

/* FILE *fopen_proc(pid_t pid, const char *fmt, ...); */
#define fopen_proc(pid, fmt, ...)					\
	({								\
		int __fd = open_proc(pid,  fmt, ##__VA_ARGS__);		\
		FILE *__f = NULL;					\
									\
		if (__fd >= 0) {					\
			__f = fdopen(__fd, "r");			\
			if (__f == NULL)				\
				pr_perror("Can't fdopen %d "		\
					"(%d/" fmt " on procfs)",	\
					__fd, pid, ##__VA_ARGS__);	\
		}							\
		__f;							\
	 })

#define pr_img_head(type, ...)	pr_msg("\n"#type __VA_ARGS__ "\n----------------\n")
#define pr_img_tail(type)	pr_msg("----------------\n")

#define DEVZERO		(makedev(1, 5))

#define KDEV_MINORBITS	20
#define KDEV_MINORMASK	((1UL << KDEV_MINORBITS) - 1)
#define MKKDEV(ma, mi)	(((ma) << KDEV_MINORBITS) | (mi))

static inline u32 kdev_major(u32 kdev)
{
	return kdev >> KDEV_MINORBITS;
}

static inline u32 kdev_minor(u32 kdev)
{
	return kdev & KDEV_MINORMASK;
}

static inline dev_t kdev_to_odev(u32 kdev)
{
	/*
	 * New kernels encode devices in a new form.
	 * See kernel's fs/stat.c for details, there
	 * choose_32_64 helpers which are the key.
	 */
	unsigned major = kdev_major(kdev);
	unsigned minor = kdev_minor(kdev);

	return makedev(major, minor);
}

extern int copy_file(int fd_in, int fd_out, size_t bytes);
extern int is_anon_link_type(char *link, char *type);

#define is_hex_digit(c)				\
	(((c) >= '0' && (c) <= '9')	||	\
	 ((c) >= 'a' && (c) <= 'f')	||	\
	 ((c) >= 'A' && (c) <= 'F'))

extern void *shmalloc(size_t bytes);
extern void shfree_last(void *ptr);

extern int cr_system(int in, int out, int err, char *cmd, char *const argv[]);
extern int cr_system_userns(int in, int out, int err, char *cmd,
				char *const argv[], int userns_pid);
extern int cr_daemon(int nochdir, int noclose, int *keep_fd, int close_fd);
extern int is_root_user(void);

static inline bool dir_dots(struct dirent *de)
{
	return !strcmp(de->d_name, ".") || !strcmp(de->d_name, "..");
}

/*
 * Size of buffer to carry the worst case or /proc/self/fd/N
 * path. Since fd is an integer, we can easily estimate one :)
 */
#define PSFDS	(sizeof("/proc/self/fd/2147483647"))

extern int read_fd_link(int lfd, char *buf, size_t size);

#define USEC_PER_SEC	1000000L
#define NSEC_PER_SEC    1000000000L

int vaddr_to_pfn(unsigned long vaddr, u64 *pfn);

/*
 * Check whether @str starts with @sub and report the
 * next character of @str in @end
 */
static inline bool strstartswith2(const char *str, const char *sub, char *end)
{
	const char *osub = sub;

	while (1) {
		if (*sub == '\0') /* end of sub -- match */ {
			if (end) {
				if (sub == osub + 1) /* pure root */
					*end = '/';
				else
					*end = *str;
			}

			return true;
		}
		if (*str == '\0') /* end of str, sub is NOT ended -- miss */
			return false;
		if (*str != *sub)
			return false;

		str++;
		sub++;
	}
}

static inline bool strstartswith(const char *str, const char *sub)
{
	return strstartswith2(str, sub, NULL);
}

/*
 * Checks whether the @path has @sub_path as a sub path, i.e.
 * sub_path is the beginning of path and the last component
 * match is full (next character terminates path component).
 *
 * Paths shouldn't contain excessive /-s, i.e. only one slash
 * between path components and no slash at the end (except for
 * the "/" path. This is pretty good assumption to what paths
 * are used by criu.
 */

static inline bool issubpath(const char *path, const char *sub_path)
{
	char end;
	return strstartswith2(path, sub_path, &end) &&
		(end == '/' || end == '\0');
}

/*
 * mkdir -p
 */
int mkdirpat(int fd, const char *path);

/*
 * Tests whether a path is a prefix of another path. This is different than
 * strstartswith because "/foo" is _not_ a path prefix of "/foobar", since they
 * refer to different directories.
 */
bool is_path_prefix(const char *path, const char *prefix);
FILE *fopenat(int dirfd, char *path, char *cflags);
void split(char *str, char token, char ***out, int *n);

int fd_has_data(int lfd);

int make_yard(char *path);

const char *ns_to_string(unsigned int ns);
#endif /* __CR_UTIL_H__ */
