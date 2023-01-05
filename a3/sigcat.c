/*
 * sigcat.c
 * 	CSSE2310 - Assignment One - 2022 - Semester
 *
 * 	Written by Erik Flink
 *
 * Usage:
 * 	sigcat
 * Arguments will be ignored.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

// The assumed maximum length of a line
#define BUFFER_SIZE 200000

// The maximum value associated with a signal
#define SIGNAL_MAX_VALUE 31

/* Function prototypes - see descriptions with the functions themselves */
void setup_sig_handler(void);
void sig_handler(int sigNum);

/*****************************************************************************/
int main(int argc, char** argv) {

    // sets up signal handlers for signals
    setup_sig_handler();
    
    char buffer[BUFFER_SIZE];

    // Infinite loop that takes input from stdin and outputs to stdout
    // Loop will stop at eof
    while (fgets(buffer, BUFFER_SIZE, stdin)) {
	printf(buffer);
	fflush(stdout);
    }
    return 0;
}

/* setup_sig_handler()
 * --------------------
 *  Sets up the signal handler for all signals of a numeric value of 1 through
 *  to 31 inclusive except for 9 (KILL) and 19 (STOP) since they are not able
 *  to be caught.
 */
void setup_sig_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = SA_RESTART;

    // loops through 1 to maximum signal value corresponding to signal and
    // sets their signal handler
    for (int signalNum = 1; signalNum <= SIGNAL_MAX_VALUE; signalNum++) {
	sigaction(signalNum, &sa, 0);
    }
}

/* sig_handler()
 * -------------
 * Prints a message according to the signal received.
 * If the signal received is SIGUSR1 then the output stream is set to stdout, 
 * if SIGUSR2 is received then the output stream is set to stderr.
 *  
 */
void sig_handler(int sigNum) {
    printf("sigcat received %s\n", strsignal(sigNum));
    fflush(stdout);

    if (sigNum == SIGUSR1) {
	dup2(STDOUT_FILENO, STDOUT_FILENO);
    } else if (sigNum == SIGUSR2) {
	dup2(STDERR_FILENO, STDOUT_FILENO);
    }
}
