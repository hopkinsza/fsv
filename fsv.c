#include <sys/wait.h>

#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define VERSION "0.0.0"

char *progname;

/* long options */
static struct option longopts[] = {
	{ "help",	no_argument,		0,		'h' },
	{ "version",	no_argument,		0,		'V' },
	{ NULL,		0,			NULL,		0 }
};

void
usage()
{
	fprintf(stderr, "usage: %s blah blah\n", progname);
}

void
version()
{
	fprintf(stderr, "fsv v%s\n", VERSION);
}

int
main(int argc, char *argv[])
{
	progname = argv[0];

	/*
	 * Process arguments
	 */

	int ch;
	while ((ch = getopt_long(argc, argv, "+hV", longopts, NULL)) != -1) {
		switch(ch) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'V':
			version();
			exit(0);
			break;
		case '?':
		default:
			usage();
			exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * Exec given cmd
	 */

	if (argc == 0) {
		fprintf(stderr, "no cmd to execute\n");
		usage();
		exit(EX_USAGE);
	}

	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1) {
		err(EX_OSERR, "cannot fork");
	} else if (pid == 0) {
		if (execvp(argv[0], argv) == -1)
			err(EX_UNAVAILABLE, "exec `%s' failed", argv[0]);
	}

	wait(&status);

	if (WIFEXITED(status))
		printf("child exited %d\n", WEXITSTATUS(status));
	else if (WIFSIGNALED(status)) {
		int sig = WTERMSIG(status);
		printf("child terminated by signal: %s (%d)\n",
		    strsignal(sig), sig);
	}

	exit(0);
}
