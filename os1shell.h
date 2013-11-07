// @author Zachary W. hancock
// @version 10/26/2013

#ifndef OS1SHELL_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

void sig_handler(int sig);

int execcomd(char* cmd);

void recordcmd(char* cmd, int sz);

void printHistory();

void addChild(pid_t pid);

void removeChild(pid_t pid);

void cleanup();

#endif
