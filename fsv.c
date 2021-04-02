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
pid_t svcpid;

volatile sig_atomic_t gotchld = 0;
volatile sig_atomic_t gotsig  = 0;

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

void
onchld(int sig)
{
	gotchld = 1;
}

void
onterm(int sig)
{
	gotsig = sig;
}

int
main(int argc, char *argv[])
{
	progname = argv[0];

	/*
	 * Block signals until ready to catch them.
	 */

	sigset_t bmask, obmask;
	struct sigaction chld_sa, term_sa;

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGTERM);

	sigprocmask(SIG_BLOCK, &bmask, &obmask);

	/*
	 * Set handlers.
	 */

	chld_sa.sa_handler = onchld;
	chld_sa.sa_mask    = bmask;
	chld_sa.sa_flags   = 0;

	term_sa.sa_handler = onterm;
	term_sa.sa_mask    = bmask;
	term_sa.sa_flags   = 0;

	sigaction(SIGCHLD, &chld_sa, NULL);
	sigaction(SIGINT,  &term_sa, NULL);
	sigaction(SIGHUP,  &term_sa, NULL);
	sigaction(SIGTERM, &term_sa, NULL);

	/*
	 * Process arguments.
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
	 * Exec given cmd.
	 */

	if (argc == 0) {
		fprintf(stderr, "no cmd to execute\n");
		usage();
		exit(EX_USAGE);
	}

	svcpid = fork();
	if (svcpid == -1) {
		err(EX_OSERR, "cannot fork");
	} else if (svcpid == 0) {
		sigprocmask(SIG_SETMASK, &obmask, NULL);
		if (execvp(argv[0], argv) == -1)
			err(EX_UNAVAILABLE, "exec `%s' failed", argv[0]);
	}

	for (;;) {
		sigsuspend(&obmask);
		if (gotchld) {
			int status;
			/* signal that the child received, if applicable */
			int csig;
			int e;

			gotchld = 0;

			while (waitpid(WAIT_ANY, &status, WNOHANG|WCONTINUED|WUNTRACED) != -1) {
				if (WIFEXITED(status)) {
					printf("child exited %d\n", WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					csig = WTERMSIG(status);
					printf("child terminated by signal: %s (%d)",
					    strsignal(csig), csig);
					if (WCOREDUMP(status))
						printf(", dumped core");
					printf("\n");
				} else if (WIFSTOPPED(status)) {
					csig = WSTOPSIG(status);
					printf("child stopped by signal: %s (%d)\n",
					    strsignal(csig), csig);
				} else if (WIFCONTINUED(status)) {
					printf("child continued\n");
				}
			}
		} else if (gotsig) {
			printf("caught signal %s (%d)\n", strsignal(gotsig), gotsig);
			gotsig = 0;
		}
	}
}
