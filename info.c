/*
 * info.c
 */

/*
 * write_info might need these
 */
// #include <sys/types.h>
// #include <sys/stat.h>

#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <sysexits.h>
#include <fcntl.h>

#include "extern.h"

struct allinfo {
	struct fsv fsv;
	struct proc procs[2];
};

void read_info(struct allinfo *ai) {
	int fd;
	size_t size = sizeof(*ai);
	ssize_t bread;

	/* TODO cleanup */
	if ((fd = open("info.struct", O_RDONLY)) == -1)
		err(EX_UNAVAILABLE, "cannot open `info.struct'");

	bread = read(fd, ai, size);
	if (bread == -1 || bread != size)
		err(EX_IOERR, "read failed");

	close(fd);
}

void write_info(struct fsv fsv, struct proc cmd, struct proc log,
		char *cmd_fullcmd, char *log_fullcmd) {

	/* strings currently ignored */

	struct allinfo ai;
	ai.fsv = fsv;
	ai.procs[0] = cmd;
	ai.procs[1] = log;

        int fd;
        size_t size = sizeof(ai);
        ssize_t written;

        /* TODO: kill child procs here? */
        if ((fd = creat("info.struct", 00666)) == -1)
                err(EX_CANTCREAT, "cannot create `info.struct'");

        written = write(fd, &ai, size);
        if (written == -1 || written != size)
                err(EX_OSERR, "write failed");

        close(fd);
}

void print_info(char *cmdname) {
	struct allinfo ai;
	read_info(&ai);

	struct fsv fsv;

	fsv = ai.fsv;

	printf("* status for %s\n", cmdname);

	/* fsv */
	printf("fsv:\n");
	printf("\trunning: %d\n",  fsv.running);
	printf("\tpid:     %ld\n", (long)fsv.pid);
	printf("\tsince:   %ld\n", (long)fsv.since.tv_sec);
	printf("\tgaveup:  %d\n",  fsv.gaveup);
	printf("\trecent_secs:         %ld\n", (long)fsv.recent_secs);
	printf("\trecent_restarts_max: %ld\n", (long)fsv.recent_restarts_max);
	printf("\ttimeout:             %ld\n", (long)fsv.timeout);

	/* procs */
	for (int i=0; i<2; i++) {
		if (i == 0)
			printf("cmd:\n");
		else
			printf("log:\n");

		struct proc p = ai.procs[i];

		printf("\tpid: %ld\n", (long)p.pid);
		printf("\ttotal_restarts:  %lu\n", p.total_restarts);
		printf("\trecent_restarts: %lu\n", p.recent_restarts);
		printf("\ttv:     %ld\n", (long)p.tv.tv_sec);
		printf("\tstatus: %d\n",  p.status);
	}
}