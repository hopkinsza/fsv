/*
 * extern.h
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

/* proc.c */

pid_t exec_str(const char *, int, int, int);
pid_t exec_argv(char *[], int, int, int);
void print_wstatus(int);

/* util.c */

void debug(char *fmt, ...);
void usage();
void version();
int mkcmddir(const char *, const char *);

#endif
