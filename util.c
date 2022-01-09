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
	fprintf(stderr, "usage: %s [options] <cmd>\n", progname);
	fprintf(stderr, "Type `man 1 fsv' for the manual.\n");
}

void
version()
{
	fprintf(stderr, "%s %s\n", progname, FSV_VERSION);
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

/*
 * Call termprocs(), update info struct, then exit.
 */
void
exitall(int status)
{
	termprocs();

	procs[0].pid = 0;
	procs[1].pid = 0;
	write_info(fd_info, *fsv, procs[0], procs[1]);
	debug("wrote info\n");

	debug("sent SIGTERM to procs, exiting\n");
	exit(status);
}

/*
 * Send SIGTERM to the two processes.
 */
void
termprocs()
{
	if (procs[0].pid != 0) {
		kill(procs[0].pid, SIGTERM);
		kill(procs[0].pid, SIGCONT);
	}
	if (procs[1].pid != 0) {
		kill(procs[1].pid, SIGTERM);
		kill(procs[1].pid, SIGCONT);
	}
}

unsigned long
str_to_ul(const char *str)
{
	char *ep;
	long val;

	errno = 0;
	val = strtol(str, &ep, 0);

	if (ep == str) {
		warnx("string to number conversion failed for `%s': "
		      "not a number, or improper format", str);
		exitall(EX_DATAERR);
	}
	if (*ep != '\0') {
		warnx("string to number conversion failed for `%s': "
		      "trailing junk `%s'", str, ep);
		exitall(EX_DATAERR);
	}
	if (errno != 0) {
		warnx("string to number conversion failed for `%s': "
		      "number out of range", str);
		exitall(EX_DATAERR);
	}
	if (val < 0) {
		warnx("string to number conversion failed for `%s': "
		      "should be positive", str);
		exitall(EX_DATAERR);
	}

	return (unsigned long)val;
}
