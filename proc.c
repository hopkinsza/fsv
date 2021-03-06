/*
 * proc.c
 *   Subroutines to fork & exec the log & cmd processes.
 *   Fork, reset signal handling, set up STD{IN,OUT,ERR}, and exec.
 *   Call exitall() if there's an error.
 *   Return PID of child to the parent.
 *
 *   `in, out, err' are the file descriptors to dup into that respective slot
 *   for the child, which will inherit them.
 *   Value of -1 means use the /dev/null file descriptor.
 */

#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "extern.h"

static void mydup2(int oldfd, int newfd) {
	/* just to reduce repeated code */

	if (oldfd == -1)
		oldfd = fd_devnull;

	if (dup2(oldfd, newfd) == -1) {
		warn("dup2(2) failed");
		exitall(EX_OSERR);
	}
}
pid_t exec_str(const char *const str, int in, int out, int err) {
	char *c;
	enum { BS = 32 };
	char *argv[BS];
	pid_t pid;

	for (int i=0; i<BS; i++) {
		argv[i] = NULL;
	}

	switch(pid = fork()) {
	case -1:
		warn("fork(2) failed");
		exitall(EX_OSERR);
		break;
	case 0:
		sigprocmask(SIG_SETMASK, &obmask, NULL);

		mydup2(in,  0);
		mydup2(out, 1);
		mydup2(err, 2);

		/*
		 * Convert a copy of the string into an argv...
		 */

		c = malloc(strlen(str) + 1);
		strcpy(c, str);

		bool done = false;
		// bool escaping = false;
		// bool inquotes = false;
		argv[0] = c;
		for (int i=0; i<BS; i++) {
			while (1) {
				if (*c == '\0') {
					done = true;
					break;
				}
				while (*c == ' ') {
					*c = '\0';
					c++;
				}

				if (*c != '"' && *c != '\0') {
					/* lovely, simple, normal argument */
					argv[i] = c;
					while (*c != '\0' && *c != ' ')
						c++;
					break;
				} else {
					/* double-quoted argument */
					c++;
					argv[i] = c;
					while (*c != '\0' && *c != '"')
						c++;
					if (*c == '\0') {
						warn("unmatched double-quote");
						exitall(EX_DATAERR);
					} else if (*c == '"') {
						*c = '\0';
						c++;
					}
					break;
				}

			}
			if (done)
				break;
		}

		if (!done) {
			warn("too many words in cmd or log");
			exitall(EX_SOFTWARE);
		}

		/*
		 * And exec it.
		 */

		if (execvp(argv[0], argv) == -1) {
			warn("exec `%s' failed", argv[0]);
			exitall(EX_OSERR);
		}

		break;

#if 0
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
#endif
	}
	return pid;
}
pid_t exec_argv(char *argv[], int in, int out, int err) {
	pid_t pid;

	switch(pid = fork()) {
	case -1:
		warn("fork(2) failed");
		exitall(EX_OSERR);
		break;
	case 0:
		sigprocmask(SIG_SETMASK, &obmask, NULL);

		mydup2(in,  0);
		mydup2(out, 1);
		mydup2(err, 2);

		if (execvp(argv[0], argv) == -1) {
			warn("exec `%s' failed", argv[0]);
			exitall(EX_OSERR);
		}
		break;
	}
	return pid;
}

/*
 * Print out some updates given the status updated from wait(2)
 * into the designated file descriptor.
 */
void
dprint_wstatus(int fd, int status)
{
	/* Signal that the child received, if applicable. */
	int csig;

	if (WIFEXITED(status)) {
		dprintf(fd, "exited %d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		csig = WTERMSIG(status);
		dprintf(fd, "terminated by signal: %s (%d)",
		    strsignal(csig), csig);
		if (WCOREDUMP(status))
			dprintf(fd, ", dumped core");
		dprintf(fd, "\n");
	} else if (WIFSTOPPED(status)) {
		csig = WSTOPSIG(status);
		dprintf(fd, "stopped by signal: %s (%d)\n",
		    strsignal(csig), csig);
	} else if (WIFCONTINUED(status)) {
		dprintf(fd, "continued\n");
	}
}
