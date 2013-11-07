/// os1shell is a simple command shell that will fork and execute commands 
/// entered by standard input. Programs may be run in the background by 
/// appending the '&' character to the end of the command or as the final
/// command parameter.  To quit the shell the user must enter an EOF,
/// CTRL+D, or send SIGQUIT signal, CTRL+\.
///
/// @author: Zachary W. Hancock
/// @version: 10/26/2013

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#include "os1shell.h"

#define SHELL_PROMPT "OS1Shell -> "
#define MAX_ARGS 10
#define HISTORY_SIZE 20

char* history[HISTORY_SIZE];
int numChildren = 0;
pid_t* children[32];
int histInd = 0;
int interrupted = 0;

int main() {

	// install signal handler
	struct sigaction sa;
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	int sig;
	for(sig = 0; sig < 32; sig++) {
		if(sig != SIGQUIT)				// allow SIGQUIT to exit as expected
			sigaction(sig, &sa, NULL);
	}
	
	// prompt and get input
	printf("%s", SHELL_PROMPT);
	fflush(stdout);

	int r;
	int const buffSize = 64;
	char buff[buffSize];
	while( (r = read(fileno(stdin), buff, buffSize)) > 0 || interrupted ) {
		
		if(r > 1 && !interrupted) {
			// replace line termination with a null termination
			char *newline = strchr( buff, '\n' );
			if ( newline ) *newline = '\0';
		
			recordcmd(buff, r);
			execcmd(buff);
		}

		interrupted = 0;
		printf("%s", SHELL_PROMPT);
		fflush(stdout);
	}

	cleanup();

	return 0;
}

/* sig_handler called asyncronously by the OS.  This function prints
 * the recieved signal to standard output.  Special cases for SIGINT,
 * print out command history, and SIGCHLD, handles termination of a child
 * process are included.
 * 		Parameters:
 *			int sig - signal number to be handled.
 */
void sig_handler(int sig) {
	int status;
	pid_t pid;
	
	if(sig == SIGINT) {
		printHistory();
	}
	else if(sig == SIGCHLD) {
		while( (pid = waitpid(-1, &status, WNOHANG)) > 0 ) {
 			removeChild(pid);
		}
	}
	else {
		printf("\nRecieved Signal:%i\n", sig);
	}
	
	// normal execution has been inturrupted.
	interrupted = 1;
}

/* Add an entry to the command history. 
 * 		Parameters:
 *			char* cmd - c string representation of user input
 *			int sz - size of the command to store
 */
void recordcmd(char* cmd, int sz) {
	char* entry;
	char* old = history[histInd];

	entry = (char*)malloc(sz * sizeof *entry);
	strcpy(entry, cmd);
	free(old);

	history[histInd] = entry;

	// increment index
	if( ++histInd > HISTORY_SIZE-1 ) histInd = 0;
}

/* Print out the contents of the stored history.
 */
void printHistory() {
	int printInd = histInd;
	
	printf("\n");

	do {	
		if(history[printInd] != NULL) {
			printf("%s\n", history[printInd]);
		}

		// increment print index	
		if( ++printInd > HISTORY_SIZE-1 ) printInd = 0;
	} while( printInd != histInd );
}

/* Attempt to execute a given command in a new process using fork and execvp.
 * Processes may be executed in the background by appending the '&' character.
 *		Parameters:
 *			char* cmd - command to execute
 *		Returns:
 *			1 - New process successfully forked.
 *		   -1 - Unable to fork a child process.
 *		** Note: Failures to execute on the new process will result in an error
 *				printed to console.  The child process will terminate.
 */
int execcmd(char* cmd) {
	char* tokens[MAX_ARGS];
	int status = 0;
	int background = 0;
	pid_t pid;

	// split into tokens
	int i = 0;
	char* token;
	token = strtok(cmd, " \r\n");
	while( i < MAX_ARGS && token != NULL ) {
		tokens[i] = token;
		token = strtok(NULL, " ");
		i++;
	}
	tokens[i] = NULL;	// end arguments with NULL.

	// & character determines if child will run in the background
	if( tokens[0][strlen(tokens[0])-1]=='&' ) {
		tokens[0][strlen(tokens[0])-1] = '\0';
		background = 1;
	}
	else if (tokens[i-1][0]=='&' && strlen(tokens[i-1])==1) {
		tokens[i-1] = NULL;
		background = 1;
	} 

	if( 0 > (pid = fork()) ) {
		fprintf(stderr, "%s\n", "Error: could not fork child process");
		return -1;
	}
	else if(0 == pid) {	// Run if child
		execvp(tokens[0], tokens);
		
		// if we get here the command could not execute
		fprintf(stderr, "%s%s\n", tokens[0], ": command not found");
		exit(-1);
	}
	else {	// if parent
		
		// wait for child to terminate, unless specified by '&' char
		if( !background ) {
			while( pid != wait(&status) );
		}
		else {
			addChild(pid);
		}
	}

	return 1;
}

/* Add a pid to the list of active child processes.  This function will block
 * child termination signals, SIGCHLD, util the pid has been recorded.
 *		Parameters:
 *			pid_t - process id of executing child process.
 */
void addChild(pid_t pid) {
	pid_t* child;
	sigset_t temp_mask, mask;

	sigemptyset(&temp_mask);
	sigaddset(&temp_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &temp_mask, &mask);	

	child = (pid_t*)malloc(sizeof(pid_t));
	*child = pid;
	children[numChildren++] = child;

	// if we run out of space create a new children of double size.
	
	sigprocmask(SIG_SETMASK, &mask, NULL);
}

/* Remove a child from the list of active child processes.
 *		Parameters:
 *			pid_t - process id of child to remove.
 */
void removeChild(pid_t pid) {
	int openIndex;
	int i;

	for(i = 0; i < numChildren; i++) {
		if( *(children[i]) == pid ) {
			free(children[i]);
			openIndex = i;
			numChildren--;
		}
		else if(openIndex) {
			children[i-1] = children[i];
		}
	}
}

/* Cleanup all child processes by sendign a SIGKILL to all pids in the current
 * list of children.
 */
void cleanup() {
	int i;
	if(numChildren > 0) {
		for(i = 0; i < numChildren; i++)
			kill( *(children[i]), SIGKILL );
	}
}
