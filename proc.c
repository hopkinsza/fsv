/*
 * proc.c
 */

#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * Subroutines to fork & exec the log & cmd processes.
 * Fork, reset signal handling, set up STD{IN,OUT,ERR}, and exec.
 * Send SIGTERM to the process group if there's an error.
 * Return PID of child to the parent.
 *
 * `in, out, err' are the file descriptors to dup into that respective
 * slot, since child inherits FDs.
 * Value of -1 means do nothing.
 * TODO: should -1 mean do nothing, or close it?
 */
static void mydup2(int oldfd, int newfd) {
	/* just to reduce repeated code */

	if (oldfd == -1)
		return;

	if (dup2(oldfd, newfd) == -1) {
		warn("dup2(2) failed");
		exitall();
	}
}
pid_t exec_str(const char *str, int in, int out, int err) {
	/* TODO: execl the shell to process str */
	pid_t pid;
	size_t l;
	char *command;

	switch(pid = fork()) {
	case -1:
		warn("fork(2) failed");
		exitall();
		break;
	case 0:
		sigprocmask(SIG_SETMASK, &obmask, NULL);

		mydup2(in,  0);
		mydup2(out, 1);
		mydup2(err, 2);

		/*
		 * Allocate buffer to hold the additional 5 bytes `exec '.
		 * strcpy(3) and strcat(3) should be ok here.
		 */
		l = strlen(str) + 1;
		l += 5;
		command = malloc(l);
		strcpy(command, "exec ");
		strcat(command, str);

		if (execl("/bin/sh", "sh", "-c", command, (char *)NULL) == -1) {
			warn("exec `%s' failed", str);
			exitall();
		}
		break;
	}
	return pid;
}
pid_t exec_argv(char *argv[], int in, int out, int err) {
	pid_t pid;

	switch(pid = fork()) {
	case -1:
		warn("fork(2) failed");
		exitall();
		break;
	case 0:
		sigprocmask(SIG_SETMASK, &obmask, NULL);

		mydup2(in,  0);
		mydup2(out, 1);
		mydup2(err, 2);

		if (execvp(argv[0], argv) == -1) {
			warn("exec `%s' failed", argv[0]);
			exitall();
		}
		break;
	}
	return pid;
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
		debug("exited %d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		csig = WTERMSIG(status);
		debug("terminated by signal: %s (%d)",
		    strsignal(csig), csig);
		if (WCOREDUMP(status))
			debug(", dumped core");
		debug("\n");
	} else if (WIFSTOPPED(status)) {
		csig = WSTOPSIG(status);
		debug("stopped by signal: %s (%d)\n",
		    strsignal(csig), csig);
	} else if (WIFCONTINUED(status)) {
		debug("continued\n");
	}
}
