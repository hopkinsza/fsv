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
 * Send SIGTERM to our process group.
 * This should only be used while SIGTERM is ignored in the calling process.
 */
void
termpgrp()
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
}

/*
 * Call termpgrp(), then exit.
 */
void
exitall()
{
	termpgrp();
	debug("sent SIGTERM to pgrp, exiting\n");
	exit(0);
}

void
cd_to_cmddir(const char *cmddir, int create)
{
	char fsvdir[32];

	/*
	 * Build name of fsvdir -- fsv-${uid}.
	 */

	int l;
	l = snprintf(fsvdir, sizeof(fsvdir), "fsv-%ld", (long)geteuid());
	if (l < 0) {
		err(EX_OSERR, "snprintf");
	} else if (l >= sizeof(fsvdir)) {
		/* should never happen */
		errx(EX_SOFTWARE, "snprintf: fsvdir too long");
	}

	/*
	 * cd to FSV_CMDDIR_PREFIX.
	 */

	if (chdir(FSV_CMDDIR_PREFIX) == -1)
		err(EX_OSFILE, "chdir to `%s' failed", FSV_CMDDIR_PREFIX);

	/*
	 * mkdir $fsvdir if necessary, cd to it.
	 */

	if (create) {
		if (mkdir(fsvdir, 00777) == -1) {
			switch (errno) {
			case EEXIST:
				/* already exists, ok */
				break;
			default:
				err(EX_IOERR, "mkdir `%s/%s' failed",
				    FSV_CMDDIR_PREFIX, fsvdir);
			}
		}
	}
	if (chdir(fsvdir) == -1)
		err(EX_IOERR, "chdir to `%s/%s' failed", FSV_CMDDIR_PREFIX, fsvdir);

	/*
	 * mkdir $cmddir if necessary, cd to it.
	 */
	if (create) {
		if (mkdir(cmddir, 00777) == -1) {
			switch (errno) {
			case EEXIST:
				/* already exists, ok */
				break;
			default:
				err(EX_IOERR, "mkdir `%s/%s/%s' failed",
				    FSV_CMDDIR_PREFIX, fsvdir, cmddir);
			}
		}
	}
	if (chdir(cmddir) == -1)
		err(EX_IOERR, "chdir to `%s/%s/%s' failed",
		    FSV_CMDDIR_PREFIX, fsvdir, cmddir);
}
