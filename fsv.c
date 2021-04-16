//#define _GNU_SOURCE

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/*
 * -n name
 */

/*
 * chpst opts:
 * -b argv0
 * -e envdir
 * -/ root
 * -n nice
 * -l lock
 *   -m bytes: memory limits
 *   -d bytes: data segment limit
 *   -o n: open fd limit
 *   -p n: process limit per uid
 *   -f bytes: output file size limit
 *   -c bytes: coredump size limit
 * -v
 * -P new process group
 * -0 close stdin
 * -1 close stdout
 * -2 close stderr
 */

static const char *progversion = "0.0.0";
static char *progname;

struct proc {
	pid_t pid;
	int status;
	struct timeval tv;
};

static volatile sig_atomic_t gotchld = 0;
static volatile sig_atomic_t termsig = 0;

void usage(), version();
void onchld(int), onterm(int);
void print_wstatus(int);
int mkcmddir(const char *, const char *);

int
main(int argc, char *argv[])
{
	char *cmdname = NULL;
	int logpipe[2];

	struct proc cmd = { 0, 0, { 0, 0 } };
	struct proc log = { 0, 0, { 0, 0 } };

	progname = argv[0];

	if (pipe(logpipe) == -1)
		err(EX_OSERR, "cannot make pipe");

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
		{ "help",	no_argument,		NULL,		'h' },
		{ "name",	required_argument,	NULL,		'n' },
		{ "version",	no_argument,		NULL,		'V' },
		{ NULL,		0,			NULL,		0 }
	};

	int ch;
	while ((ch = getopt_long(argc, argv, "+hn:V", longopts, NULL)) != -1) {
		switch(ch) {
		case 'h':
			usage();
			exit(0);
			break;
		case 'n':
			cmdname = optarg;
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
	 * Verify that a cmdname was given, make cmddir.
	 */

	if (argc == 0) {
		fprintf(stderr, "no cmd to execute\n");
		usage();
		exit(EX_USAGE);
	}

	const char *prefix = "/var/tmp";
	if (cmdname == NULL)
		cmdname = argv[0];
	int fd_cmddir;

	fd_cmddir = mkcmddir((const char*)cmdname, prefix);

	/*
	 * Run cmd.
	 */

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

	/*
	 * Main loop. Wait for signals, update cmd and log structs when received.
	 */

	for (;;) {
		sigsuspend(&obmask);
		if (gotchld) {
			int status;
			/* waitpid(2) flags */
			int wpf = WNOHANG|WCONTINUED|WUNTRACED;
			pid_t wpid;

			gotchld = 0;

			while ((wpid = waitpid(WAIT_ANY, &status, wpf)) > 0) {
				if (wpid == cmd.pid) {
					printf("cmd update:\n");
					cmd.status = status;
					// TODO: update `struct proc'
				} else if (wpid == log.pid) {
					printf("log update:\n");
					log.status = status;
				} else {
					printf("??? unknown child!\n");
				}

				print_wstatus(status);
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

/*
 * Print out some updates given the status updated from wait(2).
 */
void
print_wstatus(int status)
{
	/* Signal that the child received, if applicable. */
	int csig;

	if (WIFEXITED(status)) {
		printf("exited %d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		csig = WTERMSIG(status);
		printf("terminated by signal: %s (%d)",
		    strsignal(csig), csig);
		if (WCOREDUMP(status))
			printf(", dumped core");
		printf("\n");
	} else if (WIFSTOPPED(status)) {
		csig = WSTOPSIG(status);
		printf("stopped by signal: %s (%d)\n",
		    strsignal(csig), csig);
	} else if (WIFCONTINUED(status)) {
		printf("continued\n");
	}
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
