#ifndef __CSAPP_H__
#define __CSAPP_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
extern int h_errno;    /* Defined by BIND for DNS errors */ 
extern char **environ; /* Defined by libc */


#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
#define LISTENQ  1024  /* Second argument to listen() */

void unix_error(char *msg);
void app_error(char *msg);
pid_t Fork(void);
void Execve(const char *filename, char *const argv[], char *const envp[]);
pid_t Wait(int *status);
pid_t Waitpid(pid_t pid, int *iptr, int options);
void Kill(pid_t pid, int signum);
unsigned int Sleep(unsigned int secs);
void Pause(void);
void Setpgid(pid_t pid, pid_t pgid);
pid_t Getpgrp();
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
ssize_t sio_puts(char s[]);
void sio_error(char s[]);
ssize_t Sio_puts(char s[]);
void Close(int fd);
int Dup2(int fd1, int fd2);
char *Fgets(char *ptr, int n, FILE *stream);
void Free(void *ptr);
#endif /* __CSAPP_H__ */
/* $end csapp.h */
