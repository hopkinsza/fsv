/*
 * util.c
 */

#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "extern.h"

void
debug(char *fmt, ...)
{
	if (verbose) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

void
usage()
{
	fprintf(stderr, "usage: %s blah blah\n", progname);
}

void
version()
{
	fprintf(stderr, "fsv v%s\n", progversion);
}

/*
 * Send SIGTERM to our process group, then exit.
 * This should only be used while SIGTERM is ignored in the calling process.
 */
void
exitall()
{
	sigset_t cur_bmask;
	sigprocmask(SIG_BLOCK, NULL, &cur_bmask);

	if (!sigismember(&cur_bmask, SIGTERM)) {
		/*
		 * This should never happen, but print a warning and
		 * do our best if it does.
		 */
		warnx("exitall() used incorrectly in source (while SIGTERM is not ignored)");
		kill(-pgrp, SIGCONT);
		kill(-pgrp, SIGTERM);
		/* NOTREACHED */
		exit(1);
	}

	kill(-pgrp, SIGTERM);
	kill(-pgrp, SIGCONT);
	debug("sent SIGTERM to pgrp, exiting\n");
	exit(0);
}

/*
 * This function essentially does the following:
 * mkdir ${prefix}/fsv-${uid}
 * mkdir ${prefix}/fsv-${uid}/${cmddir}
 *
 * Returns fd to the new cmddir.
 */
int
mkcmddir(const char *cmddir, const char *prefix)
{
	int fd_prefix;
	int fd_fsvdir;
	int fd_cmddir;
	char fsvdir[32];

	/* build name of fsvdir */
	int l;
	l = snprintf(fsvdir, sizeof(fsvdir), "fsv-%ld", (long)geteuid());
	if (l < 0) {
		err(EX_OSERR, "snprintf");
	} else if (l >= sizeof(fsvdir)) {
		/* should never happen */
		errx(EX_SOFTWARE, "snprintf: fsvdir too long");
	}

	/* open $prefix */
	if ((fd_prefix = open(prefix, O_RDONLY|O_DIRECTORY)) == -1)
		err(EX_OSFILE, "can't open `%s'", prefix);

	/* mkdir $prefix/fsv-$uid */
	if (mkdirat(fd_prefix, fsvdir, 00777) == -1) {
		switch (errno) {
		case EEXIST:
			/* already exists, ok */
			break;
		default:
			err(EX_IOERR, "can't mkdir `%s/%s'", prefix, fsvdir);
		}
	}
	if ((fd_fsvdir = openat(fd_prefix, fsvdir, O_RDONLY|O_DIRECTORY)) == -1)
		err(EX_IOERR, "can't open `%s/%s' as directory", prefix, fsvdir);

	close(fd_prefix);

	/* mkdir $prefix/fsv-$uid/$cmddir */
	if ((mkdirat(fd_fsvdir, cmddir, 00777)) == -1) {
		switch (errno) {
		case EEXIST:
			/* already exists, ok */
			break;
		default:
			err(EX_IOERR, "can't mkdir `%s/%s/%s'", prefix, fsvdir, cmddir);
		}
	}
	if ((fd_cmddir = openat(fd_fsvdir, cmddir, O_RDONLY|O_DIRECTORY)) == -1)
		err(EX_IOERR, "can't open `%s/%s/%s' as directory", prefix, fsvdir, prefix);

	close(fd_fsvdir);

	return fd_cmddir;
}
