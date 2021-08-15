/*
 * info.c
 */

#include <err.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <sysexits.h>
#include <fcntl.h>

#include "extern.h"

struct allinfo {
	struct fsv fsv;
	struct proc procs[2];
};

/* indented printf */
static void ind_printf(char *fmt, ...);

void
read_info(struct allinfo *ai)
{
	int fd;
	size_t size = sizeof(*ai);
	ssize_t bread;

	/* TODO cleanup */
	if ((fd = open("info.struct", O_RDONLY)) == -1) {
		warn("open `info.struct' failed");
		exitall(EX_UNAVAILABLE);
	}

	bread = read(fd, ai, size);
	if (bread == -1 || bread != size) {
		warn("read from `info.struct' failed");
		exitall(EX_UNAVAILABLE);
	}

	close(fd);
}

void
write_info(struct fsv fsv, struct proc cmd, struct proc log,
           char *cmd_fullcmd, char *log_fullcmd)
{

	/* strings currently ignored */

	struct allinfo ai;
	ai.fsv = fsv;
	ai.procs[0] = cmd;
	ai.procs[1] = log;

	int fd;
	size_t size = sizeof(ai);
	ssize_t written;

	if ((fd = creat("info.struct", 00666)) == -1) {
		warn("create `info.struct' failed");
		exitall(EX_CANTCREAT);
	}

	written = write(fd, &ai, size);
	if (written == -1 || written != size) {
		warn("write into `info.struct' failed");
		exitall(EX_OSERR);
	}

	close(fd);
}

static void
ind_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("  ");
	vprintf(fmt, ap);
	va_end(ap);
}

void
print_info(char *cmdname)
{
	enum { BS = 64 };
	char buf[BS];

	struct allinfo ai;
	read_info(&ai);

	struct fsv fsv;
	fsv = ai.fsv;

	printf("status for %s\n", cmdname);

	/* fsv */
	printf("fsv:\n");

	ind_printf("%8s: ", "pid");
	if (fsv.pid == 0)
		printf("n/a\n");
	else
		printf("%ld\n", (long)fsv.pid);

	ind_printf("%8s: ", "since");
	strftime(buf, BS, FSV_DATETIME_FMT, localtime(&fsv.since.tv_sec));
	printf("%s\n", buf);
	strftime(buf, BS, "%F %T %Z", gmtime(&fsv.since.tv_sec));
	ind_printf("%8s  %s\n",  "", buf);
	ind_printf("%8s  %ld\n", "", (long)fsv.since.tv_sec);

	ind_printf("%8s: %lu\n", "timeout", fsv.timeout);
	ind_printf("%8s: %d\n", "gaveup", fsv.gaveup);

	/* procs */
	for (int i=0; i<2; i++) {
		if (i == 0)
			printf("cmd:\n");
		else
			printf("log:\n");

		struct proc p = ai.procs[i];

		ind_printf("%8s: %ld\n", "pid", (long)p.pid);
		ind_printf("%8s: %lu\n", "total_restarts", p.total_restarts);
		ind_printf("%8s: %lu\n", "recent_secs", (unsigned long)p.recent_secs);
		ind_printf("%8s: %lu\n", "recent_restarts", p.recent_restarts);
		ind_printf("%8s: %lu\n", "recent_restarts_max", p.recent_restarts_max);
		ind_printf("%8s: %ld\n", "tv", (long)p.tv.tv_sec);
		/* flush to prevent output buffering issues */
		ind_printf("%8s: ", "status");
		fflush(stdout);
		if (p.tv.tv_sec != 0)
			dprint_wstatus(1, p.status);
		else
			dprintf(1, "n/a\n");
	}
}
