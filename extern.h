/*
 * extern.h
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#define FSV_VERSION "0.0.0"

/*
 * The prefix to keep state information under.
 * This should be a "sticky" directory.
 */
#ifndef FSV_CMDDIR_PREFIX
#define FSV_CMDDIR_PREFIX "/var/tmp"
#endif

/*
 * In the US, local time is typically in AM/PM format,
 * but the %X strftime(3) conversion specifier uses 24-hour time in this locale.
 * Set this to "%F %X %Z" if you want the "preferred" time representation for
 * your locale,
 * or even "%c" if you want the "preferred" time AND date representation.
 */
#ifndef FSV_DATETIME_FMT
#define FSV_DATETIME_FMT "%F %r %Z"
#endif

#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

extern bool verbose;
extern const char *progname;
/* Process group for supervisor and children */
extern pid_t pgrp;
/* Signal block mask: new and original */
extern sigset_t bmask, obmask;

struct fsv {
	/* Is fsv running, what's the PID, and since when */
	bool running;
	pid_t pid;
	struct timeval since;
	/*
	 * Only relevant if not running.
	 * True if fsv "gave up" on restarting one of
	 * the programs because its timeout was set to 0
	 */
	bool gaveup;
};

struct proc {
	pid_t pid;
	unsigned long total_restarts;

	time_t recent_secs;
	time_t timeout;
	unsigned long recent_restarts;
	unsigned long recent_restarts_max;
	/* Time of the most recent restart */
	struct timeval tv;
	/* If non-zero, most recent exit status as returned by wait(2) */
	int status;
};

/*
 * info.c
 */

void write_info(struct fsv, struct proc, struct proc,
		char *, char *);
void print_info(char *);

/*
 * proc.c
 */

pid_t exec_str(const char *const, int, int, int);
pid_t exec_argv(char *[], int, int, int);
void dprint_wstatus(int, int);

/*
 * util.c
 */

void debug(char *fmt, ...);
void usage();
void version();

void cd_to_cmddir(const char *cmddir, int create);
void exitall(int status);
void termpgrp();

unsigned long str_to_ul(const char *);

#endif
