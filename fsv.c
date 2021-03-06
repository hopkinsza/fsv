//#define _GNU_SOURCE

#include <sys/file.h> /* for flock(2) on linux */
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "extern.h"

/*
 * probably:
 *   -k sig: signal -- pass args to kill(1)?
 *
 * maybe:
 *   -d dir: chdir
 *   -f: same, maybe with mask arg
 *
 * chpst opts:
 * -/ root
 * (limit options skipped)
 * -P new process group
 */

struct fsv fsv_real;

/* externs */
bool verbose = true;
const char *progname = NULL;
sigset_t bmask, obmask;

int fd_devnull = 0;
int fd_info = 0;

struct fsv  *fsv = &fsv_real;
struct proc procs[2];
/* * */

static volatile sig_atomic_t gotalrm = 0;
static volatile sig_atomic_t gotchld = 0;
static volatile sig_atomic_t termsig = 0;

void onalrm(int), onchld(int), onterm(int);

int  open_infostruct();
void run_cmd(struct proc *, int[], bool, int, char *[], unsigned long);
void run_log(struct proc *, int[], const char *);

int
main(int argc, char *argv[])
{
	bool do_fork = false;
	bool logging = false;
	unsigned long out_mask = 3;

	char *cmdname = NULL;
	char *log_fullcmd = NULL;
	int logpipe[2];

	struct fsv *fsv = &fsv_real;

	struct proc *cmd = &procs[0];
	struct proc *log = &procs[1];

	*fsv = (struct fsv){
		.pid = 0,
		.since = { 0, 0 },
		.timeout = 0,
		.gaveup = false
	};
	gettimeofday(&fsv->since, NULL);

	*cmd = (struct proc){
		.pid = 0,
		.total_execs = 0,
		.recent_secs = 3600, /* 1 hr */
		.recent_restarts = 1,
		.recent_restarts_max = 3,
		.tv = { 0, 0 },
		.status = 0
	};
	*log = (struct proc){
		.pid = 0,
		.total_execs = 0,
		.recent_secs = 3600,
		.recent_restarts = 1,
		.recent_restarts_max = 3,
		.tv = { 0, 0 },
		.status = 0
	};

	if (pipe(logpipe) == -1)
		err(EX_OSERR, "cannot make pipe");

	progname = argv[0];

	/*
	 * Block signals until ready to catch them.
	 */

	/* first unblock everything */
	sigemptyset(&obmask);
	sigprocmask(SIG_SETMASK, &obmask, NULL);

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGALRM);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGTERM);
	sigaddset(&bmask, SIGQUIT);

	sigprocmask(SIG_BLOCK, &bmask, NULL);

	/*
	 * Set handlers.
	 */

	struct sigaction alrm_sa, chld_sa, term_sa;

	alrm_sa.sa_handler = onalrm;
	alrm_sa.sa_mask = bmask;
	alrm_sa.sa_flags = 0;

	chld_sa.sa_handler = onchld;
	chld_sa.sa_mask    = bmask;
	chld_sa.sa_flags   = 0;

	term_sa.sa_handler = onterm;
	term_sa.sa_mask    = bmask;
	term_sa.sa_flags   = 0;

	sigaction(SIGALRM, &alrm_sa, NULL);
	sigaction(SIGCHLD, &chld_sa, NULL);
	sigaction(SIGINT,  &term_sa, NULL);
	sigaction(SIGHUP,  &term_sa, NULL);
	sigaction(SIGTERM, &term_sa, NULL);
	sigaction(SIGQUIT, &term_sa, NULL);

	/*
	 * Process arguments.
	 */

	const char *getopt_str = "+bhl:m:n:p:qR:r:s:t:vV";

	struct option longopts[] = {
		{ "background",	no_argument,		NULL,	'b' },
		{ "help",	no_argument,		NULL,	'h' },
		{ "log",	required_argument,	NULL,	'l' },
		{ "mask",	required_argument,	NULL,	'm' },
		{ "name",	required_argument,	NULL,	'n' },
		{ "pids",	required_argument,	NULL,	'p' },
		{ "quiet",	no_argument,		NULL,	'q' },
		{ "recent-secs",required_argument,	NULL,	'R' },
		{ "restarts-max",required_argument,	NULL,	'r' },
		{ "status",	required_argument,	NULL,	's' },
		{ "timeout",	required_argument,	NULL,	't' },
		{ "verbose",	no_argument,		NULL,	'v' },
		{ "version",	no_argument,		NULL,	'V' },
		{ NULL,		0,			NULL,	0 }
	};

	int ch;
	while ((ch = getopt_long(argc, argv, getopt_str, longopts, NULL)) != -1) {
		switch(ch) {
		case 'b':
			do_fork = true;
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 'l':
			logging = true;
			log_fullcmd = optarg;
			break;
		case 'm':
			out_mask = str_to_ul(optarg);
			if (out_mask == 0 || out_mask > 3)
				errx(EX_DATAERR, "-m arg must be in range 0-3");
			break;
		case 'n':
			cmdname = optarg;
			break;
		case 'p':
			cmdname = optarg;
			cd_to_cmddir(cmdname, 0);
			fd_info = open_infostruct();
			print_info_pids(fd_info, cmdname);
			exit(0);
			break;
		case 'q':
			verbose = false;
			break;
		case 'r':
			if (*optarg == 'l') {
				debug("setting recent_restarts_max for log\n");
				optarg++;
				log->recent_restarts_max = str_to_ul(optarg);
			} else {
				cmd->recent_restarts_max = str_to_ul(optarg);
			}
			break;
		case 's':
			cmdname = optarg;
			cd_to_cmddir(cmdname, 0);
			fd_info = open_infostruct();
			print_info(fd_info, cmdname);
			exit(0);
			break;
		case 'S':
			if (*optarg == 'l') {
				debug("setting recent_secs for log\n");
				optarg++;
				log->recent_secs = str_to_ul(optarg);
			} else {
				cmd->recent_secs = str_to_ul(optarg);
			}
			break;
		case 't':
			fsv->timeout = str_to_ul(optarg);
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
	 * Populate cmdname if not overridden by -n.
	 */

	if (argc == 0) {
		warnx("no cmd to execute");
		usage();
		exit(EX_USAGE);
	}

	if (cmdname == NULL) {
		/* basename(3) may modify its argument, so use a copy. */
		char *tmp;

		tmp = malloc(strlen(argv[0]) + 1);
		strcpy(tmp, argv[0]);

		/*
		 * This malloc() is never freed.
		 * Have room for an extra character because basename(3) returns
		 * `.' if given empty string.
		 */
		cmdname = malloc(strlen(tmp) + 2);

		strcpy(cmdname, basename(tmp));
	}

	debug("cmdname is %s\n", cmdname);

	if (*cmdname == '.' || *cmdname == '/') {
		errx(EX_DATAERR, "cmdname does not make sense");
	}

	/*
	 * cd to cmddir, creating if necessary.
	 */

	cd_to_cmddir(cmdname, 1);

	/*
	 * Daemonize if necessary.
	 */

	if (do_fork) {
		switch (fork()) {
		case -1:
			err(EX_OSERR, "fork(2) failed");
			break;
		case 0:
			/* Child: continue. */
			break;
		default:
			exit(0);
		}

		if (setsid() == -1)
			err(EX_OSERR, "setsid(2) failed");
	}
	/* Set my pid now, because the fork may have changed it. */
	fsv->pid = getpid();

	/*
	 * Open /dev/null.
	 */

	if ((fd_devnull = open("/dev/null", O_RDWR|O_CLOEXEC, 00666)) == -1)
		err(EX_UNAVAILABLE, "open `/dev/null' failed");

	/*
	 * Open and flock `info.struct'.
	 */

	fd_info = open_infostruct();

	if (flock(fd_info, LOCK_EX|LOCK_NB) == -1)
		err(EX_UNAVAILABLE, "flock(2) failed (already running?)");

	/*
	 * Run log process (if applicable) and cmd.
	 * From this point onward, only use exitall() to exit, because we have
	 * child processes to cleanup.
	 */

	if (logging) {
		run_log(log, logpipe, log_fullcmd);
	}

	run_cmd(cmd, logpipe, logging, argc, argv, out_mask);

	write_info(fd_info, *fsv, *cmd, *log);

	/*
	 * Main loop. Wait for signals, update cmd and log structs when received.
	 * Not indented because it was getting cramped.
	 */

	for (;;) {

	sigsuspend(&obmask);

	if (gotalrm) {
		/* Alarm received, timeout is done. */
		gotalrm = 0;
		run_cmd(cmd, logpipe, logging, argc, argv, out_mask);
	} else if (gotchld) {
		int status;
		pid_t wpid;

		time_t now;
		struct proc *proc;

		gotchld = 0;

		while ((wpid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0) {
			if (wpid != log->pid && wpid != cmd->pid) {
				/* should never happen */
				debug("??? unknown child!\n");
			}

			for (int i=0; i<2; i++) if (wpid == procs[i].pid) {
				bool do_restart = true;
				const char *procname;

				if (i == 0)
					procname = "cmd";
				else if (i == 1)
					procname = "log";

				if (verbose) {
					debug("%s update: ", procname);
					dprint_wstatus(2, status);
				}

				proc = &procs[i];
				proc->status = status;
				time(&now);

				if (proc->recent_secs == 0 ||
				    ((now - proc->tv.tv_sec) <= proc->recent_secs)) {
					proc->recent_restarts += 1;
				} else {
					proc->recent_restarts = 1;
				}

				if (proc->recent_restarts > proc->recent_restarts_max) {
					/* Max restarts exceeded. */
					do_restart = false;
					debug("recent_restarts_max (%lu) exceeded for %s, ",
					    proc->recent_restarts_max, procname);

					if (i == 1 || fsv->timeout == 0) {
						debug("exiting\n");
						fsv->pid = 0;
						fsv->gaveup = true;
						/* exitall() will call write_info */
						exitall(0);
					} else {
						debug("setting alarm for %lu secs\n",
						    fsv->timeout);
						alarm((unsigned int)fsv->timeout);
					}
				}

				if (do_restart && i == 0) {
					/* restart cmd */
					run_cmd(cmd, logpipe, logging,
						argc, argv, out_mask);
				} else if (do_restart && i == 1) {
					/* restart log */
					run_log(log, logpipe, log_fullcmd);
				}
			}
		}
		write_info(fd_info, *fsv, *cmd, *log);
	} else if (termsig) {
		debug("\ncaught signal %s (%d)\n",
		    strsignal(termsig), termsig);
		exitall(EX_OSERR);
	}

	}
}

/*
 * Only use before spawning any child processes, does not exit with exitall().
 */
int
open_infostruct()
{
	int fd;
	if ((fd = open("info.struct", O_CREAT|O_RDWR|O_CLOEXEC, 00666)) == -1)
		err(EX_UNAVAILABLE, "open `info.struct' failed");

	return fd;
}

void
run_cmd(struct proc *cmd, int logpipe[], bool logging,
	int argc, char *argv[], unsigned long out_mask)
{
	int fd0, fd1, fd2;

	cmd->total_execs++;
	gettimeofday(&cmd->tv, NULL);

	fd0 = -1;
	fd1 = -1;
	fd2 = -1;
	if (logging) {
		switch (out_mask) {
		case 1:
			fd1 = logpipe[1];
			break;
		case 2:
			fd2 = logpipe[1];
			break;
		case 3:
			fd1 = logpipe[1];
			fd2 = logpipe[1];
			break;
		}
	}

	cmd->pid = exec_argv(argv, fd0, fd1, fd2);

	debug("started cmd process (%ld) at %ld\n", cmd->pid, (long)cmd->tv.tv_sec);
}

void
run_log(struct proc *log, int logpipe[], const char *log_fullcmd)
{
	log->total_execs++;
	gettimeofday(&log->tv, NULL);
	log->pid = exec_str(log_fullcmd, logpipe[0], -1, -1);

	debug("started log process (%ld) at %ld\n", log->pid, (long)log->tv.tv_sec);
}

void
onalrm(int sig)
{
	gotalrm = 1;
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
