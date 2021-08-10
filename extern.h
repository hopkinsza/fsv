/*
 * extern.h
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

extern bool verbose;
extern const char *progversion;
extern char *progname;
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
	 * Only relevant if not running. True if fsv "gave up" on restarting
	 * the program because timeout is set to 0 (see next)
	 */
	bool gaveup;
	/*
	 * The following 3 values are related to the programs being run.
	 *
	 * Max amount of seconds before considering this a new string of crashes
	 *   i.e. resetting recent_restarts to 0 (value of 0 means never);
	 * How many recent restarts to allow before taking action;
	 * What action to take:
	 *   0: stop restarting
	 *   n: wait n secs before restarting again
	 */
	time_t recent_secs;
	time_t recent_restarts_max;
	time_t timeout;
};

struct proc {
	pid_t pid;
	unsigned long total_restarts;
	unsigned long recent_restarts;
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

pid_t exec_str(const char *, int, int, int);
pid_t exec_argv(char *[], int, int, int);
void print_wstatus(int);

/*
 * util.c
 */

void debug(char *fmt, ...);
void usage();
void version();
int mkcmddir(const char *, const char *);

#endif
