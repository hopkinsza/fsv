//#define _GNU_SOURCE

#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

/*
 * implemented:
 *   -c 'cmd'
 *   -h
 *   -l 'logproc'
 *   -n 'name'
 *   -v
 *   -V
 *
 * planned flags:
 *   -s name: status
 *
 * probably:
 *   -k sig: signal -- pass args to kill(1)?
 *
 * maybe:
 *   -d dir: chdir
 *   -m: as in freebsd daemon(8)
 *   -f: same, maybe with mask arg
 *
 * chpst opts:
 * -b argv0
 * -e envdir
 * -/ root
 * -n nice
 * -l lock
 * (limit options skipped)
 * -v
 * -P new process group
 * -0 close stdin
 * -1 close stdout
 * -2 close stderr
 */

bool verbose = true;
const char *progversion = "0.0.0";
char *progname;
pid_t pgrp;
sigset_t bmask, obmask;

static volatile sig_atomic_t gotchld = 0;
static volatile sig_atomic_t termsig = 0;

void onchld(int), onterm(int);

int
main(int argc, char *argv[])
{
	bool logging = false;
	bool status = false;

	char *cmdname = NULL;	/* Extracted from argv or -c arg */
	char *cmd_fullcmd = NULL;
	char *log_fullcmd = NULL;
	int logpipe[2];

	struct fsv fsv_real;
	struct fsv *fsv = &fsv_real;
	/* procs[0] is cmd process, procs[1] is log process */
	struct proc procs[2];
	struct proc *cmd = &procs[0];
	struct proc *log = &procs[1];

	progname = argv[0];

	*fsv = (struct fsv){
		.running = true,
		.pid = getpid(),
		.since = { 0, 0 },
		.gaveup = false,
		.recent_secs = 3600, /* 1 hr */
		.recent_restarts_max = 2,
		.timeout = 0
	};
	gettimeofday(&fsv->since, NULL);

	for (int i=0; i<2; i++) {
		procs[i] = (struct proc){
			.pid = 0,
			.total_restarts = 0,
			.recent_restarts = 0,
			.tv = { 0, 0 },
			.status = 0
		};
	}

#if 0
	/* Fork to make sure we're not a process group leader. */
	switch (fork()) {
	case -1:
		err(EX_OSERR, "cannot fork");
		break;
	case 0:
		/* Child: continue. */
		break;
	default:
		exit(0);
	}

	if ((pgrp = setsid()) == -1)
		err(EX_OSERR, "cannot setsid(2)");
#endif

	if (getpgid(0) != getpid()) {
		errx(1, "not a process group leader");
	}

	if (pipe(logpipe) == -1)
		err(EX_OSERR, "cannot make pipe");

	/*
	 * Block signals until ready to catch them.
	 */

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

	const char *getopt_str = "+chl:n:qs:vV";

	struct option longopts[] = {
		{ "cmd",	required_argument,	NULL,		'c' },
		{ "help",	no_argument,		NULL,		'h' },
		{ "log",	required_argument,	NULL,		'l' },
		{ "name",	required_argument,	NULL,		'n' },
		{ "quiet",	no_argument,		NULL,		'q' },
		{ "status",	required_argument,	NULL,		's' },
		{ "verbose",	no_argument,		NULL,		'v' },
		{ "version",	no_argument,		NULL,		'V' },
		{ NULL,		0,			NULL,		0 }
	};

	int ch;
	while ((ch = getopt_long(argc, argv, getopt_str, longopts, NULL)) != -1) {
		switch(ch) {
		case 'c':
			cmd_fullcmd = optarg;
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 'l':
			logging = true;
			log_fullcmd = optarg;
			break;
		case 'n':
			cmdname = optarg;
			break;
		case 'q':
			verbose = false;
			break;
		case 's':
			status = true;
			cmdname = optarg;
			break;
		case 'v':
			verbose = true;
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
	 * Verify that we have a command to run.
	 * This can be either cmd_fullcmd from -c, or argv, but not both.
	 * Then populate cmdname if not overridden by -n.
	 */

	/* TODO2: make this not garbage */
	if (status != true) {

	if (cmd_fullcmd != NULL) {
		/* Given via -c. */
		if (argc > 0) {
			warnx("was given `-c' in addition to command");
			usage();
			exit(EX_USAGE);
		}

		if (cmdname == NULL) {
			/*
			 * Use portion of the string up to the first space.
			 * This is equivalent to the command as long as it
			 * doesn't contain a space, and what kind of maniac
			 * would do that?
			 */
			cmdname = malloc(strlen(cmd_fullcmd) + 1);
			strcpy(cmdname, cmd_fullcmd);
			strtok(cmdname, " ");
		}
	} else {
		/* Not given via -c, use argv instead. */
		if (argc == 0) {
			warnx("no cmd to execute");
			usage();
			exit(EX_USAGE);
		}

		if (cmdname == NULL)
			cmdname = argv[0];
	}

	}

	debug("cmdname is %s\n", cmdname);

	/*
	 * Make cmddir and chdir to it.
	 */

	{
		const char *prefix = "/var/tmp";
		int cmddir_fd;

		cmddir_fd = mkcmddir(cmdname, prefix);
		fchdir(cmddir_fd);
		close(cmddir_fd);
	}

	/*
	 * Status, if applicable.
	 */

	/* TODO2 */
	if (status) {
		print_info(cmdname);
		exit(0);
	}

	/*
	 * Run log process, if applicable.
	 */

	if (logging) {
		gettimeofday(&log->tv, NULL);
		/* TODO1: add flag to control which fd's to pass through the pipe */
		log->pid = exec_str(log_fullcmd, logpipe[0], -1, -1);

		debug("started log process (%ld) at %ld\n", log->pid, (long)log->tv.tv_sec);
	}

	/*
	 * Run cmd.
	 */

	gettimeofday(&cmd->tv, NULL);

	{
		int fd0, fd1, fd2;
		fd0 = -1;
		if (logging) {
			fd1 = logpipe[1];
			fd2 = logpipe[1];
		} else {
			fd1 = -1;
			fd2 = -1;
		}

		/* TODO1 */
		if (argv > 0)
			cmd->pid = exec_argv(argv, fd0, fd1, fd2);
		else
			cmd->pid = exec_str(cmd_fullcmd, fd0, fd1, fd2);
	}

	debug("started cmd process (%ld) at %ld\n", cmd->pid, (long)cmd->tv.tv_sec);

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

			time_t now;
			struct proc *proc;

			gotchld = 0;

			while ((wpid = waitpid(WAIT_ANY, &status, wpf)) > 0) {
				if (wpid != log->pid && wpid != cmd->pid) {
					debug("??? unknown child!\n");
				}

				for (int i=0; i<2; i++) {
					if (wpid == procs[i].pid) {
						if (i == 0)
							debug("cmd exited:\n");
						else if (i == 1)
							debug("log exited:\n");

						proc = &procs[i];
						proc->total_restarts += 1;
						time(&now);

						if (fsv->recent_secs == 0 ||
						    ((now - proc->tv.tv_sec) <= fsv->recent_secs)) {
							// oop
							proc->recent_restarts += 1;
						} else {
							// re
							proc->recent_restarts = 1;
						}

						if (proc->recent_restarts >=
						    fsv->recent_restarts_max) {
							// TODO
							// implement timeout if timeout>0
							fsv->gaveup = true;
							kill(cmd->pid, SIGTERM);
							kill(cmd->pid, SIGCONT);
							kill(log->pid, SIGTERM);
							kill(log->pid, SIGCONT);
						}
					}
				}

				/* TODO */
				write_info(*fsv, *cmd, *log, NULL, NULL);
				print_wstatus(status);
				debug("\n");
			}
		} else if (termsig) {
			debug("\ncaught signal %s (%d)\n",
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
onchld(int sig)
{
	gotchld = 1;
}

void
onterm(int sig)
{
	termsig = sig;
}
