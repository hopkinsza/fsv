#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static const char *progversion = "0.0.0";
static char *progname;

struct proc {
	pid_t pid;
	int status;
	struct timeval tv;
};

static struct proc cmd = { 0, 0, { 0, 0 } };
static struct proc log = { 0, 0, { 0, 0 } };

static volatile sig_atomic_t gotchld = 0;
static volatile sig_atomic_t termsig = 0;

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

void
onchld(int sig)
{
	gotchld = 1;
}

void
onterm(int sig)
{
	termsig = sig;
}

int
main(int argc, char *argv[])
{
	progname = argv[0];

	/*
	 * Block signals until ready to catch them.
	 */

	sigset_t bmask, obmask;

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGTERM);
	sigaddset(&bmask, SIGQUIT);

	sigprocmask(SIG_BLOCK, &bmask, &obmask);

	/*
	 * Set handlers.
	 */

	struct sigaction chld_sa, term_sa;

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
	sigaction(SIGQUIT, &term_sa, NULL);

	/*
	 * Process arguments.
	 */

	struct option longopts[] = {
		{ "help",	no_argument,		0,		'h' },
		{ "version",	no_argument,		0,		'V' },
		{ NULL,		0,			NULL,		0 }
	};

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

	gettimeofday(&cmd.tv, NULL);
	switch(cmd.pid = fork()) {
	case -1:
		err(EX_OSERR, "cannot fork");
		break;
	case 0:
		/* Child: reset signal handling and exec */
		sigprocmask(SIG_SETMASK, &obmask, NULL);
		if (execvp(argv[0], argv) == -1)
			err(EX_UNAVAILABLE, "exec `%s' failed", argv[0]);
		break;
	}

	printf("child started at %d\n", cmd.tv.tv_sec);

	for (;;) {
		sigsuspend(&obmask);
		if (gotchld) {
			int status;
			/* waitpid(2) flags */
			int wpf = WNOHANG|WCONTINUED|WUNTRACED;
			pid_t wpid;
			/* signal that the child received, if applicable */
			int csig;

			gotchld = 0;

			while ((wpid = waitpid(WAIT_ANY, &status, wpf)) != -1) {
				if (wpid == cmd.pid) {
					printf("cmd update:\n");
				} else if (wpid == log.pid) {
					printf("log update:\n");
				} else {
					printf("??? unknown child!\n");
				}

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
				printf("\n");
			}
		} else if (termsig) {
			printf("caught signal %s (%d)\n",
			    strsignal(termsig), termsig);
			/*
			 * Restore default handler, unblock signals, and raise
			 * it again.
			 */
			term_sa.sa_handler = SIG_DFL;
			sigaction(termsig, &term_sa, NULL);
			sigprocmask(SIG_UNBLOCK, &bmask, NULL);
			raise(termsig);
			/* NOTREACHED */
			termsig = 0;
		}
	}
}
