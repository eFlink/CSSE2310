/*
 * hq.c
 * 	CSSE2310 - Assignment three - 2022 - Semester One
 *
 * 	Written by Erik Flink
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h> 
#include <string.h>
#include <csse2310a3.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>

// highest signal number
#define MAXIMUM_SIGNAL 31

// lowest signal number
#define MINIMUM_SIGNAL 1

// newline character
#define NEWLINE '\n'

// string terminator
#define TERMINATOR '\0'

// The amount to multiply microseconds to seconds
#define MICROSECONDS 1000000

// Structure type to hold process information
typedef struct {
    char* name;
    pid_t pid;
    int fdRead[2];
    int fdWrite[2];
    FILE* readText;
    int status;
} Process;

// Structure type to hold commands entered into stdin
typedef struct {
    char** cmdArgs;
    int argCount;
} Arguments;

/* Function prototypes - see descriptions with the functions themselves */
void process_inputs(void);
Process* process_command(Arguments arguments, Process* processes, 
	int* procSize);
Process* spawn_program(Arguments arguments, Process* processes, int* procSize);
void report_programs(Arguments arguments, Process* processes, 
	int procSize);
void print_status(Process* processes, int jobID);
void send_signal(Arguments arguments, Process* processes, int procSize);
void sleep_command(Arguments arguments);
void send_text(Arguments arguments, Process* processes, int procSize);
void receive_text(Arguments arguments, Process* processes, int procSize);
void eof(Arguments arguments, Process* processes, int procSize);
void cleanup(Process* processes, int procSize);
bool valid_job(char* jobID, int procSize);
bool is_number(char* number);
void setup_sig_handler(void);
void sig_handler(int sigNum);

/*****************************************************************************/
int main(int argc, char** argv) {

    // sets up signal handler to ignore SIGINT
    setup_sig_handler();
    
    process_inputs();

    return 0;
}

/* process_inputs()
 * ------------------
 * loops and reads commands given from stdin then proceeds to process commands.
 * Only stops if EOF is detected.
 */
void process_inputs(void) {
    
    Arguments arguments;
    
    // create list of process struct with memset
    Process* processes = malloc(sizeof(Process));
    int procSize = 0;

    char* line;

    do {
	printf("> ");
	fflush(stdout);
	
	line = read_line(stdin);

	// check if eof is detected
	if (line == NULL) {
	    break;
	}

	// split line by space and not quotes, put list into arguments struct
	arguments.cmdArgs = split_space_not_quote(line, &arguments.argCount);

	// if no inputs are given, go through loop again
	if (arguments.argCount == 0) {
	    free(arguments.cmdArgs);
	    free(line);
	    continue;
	} 
	// process command and returns updated processes with information
	// of child processes
	processes = process_command(arguments, processes, 
		&procSize);
	// free memory
	free(arguments.cmdArgs);
	free(line);
    } while (true);
    // frees memory, closes child processes and close files
    cleanup(processes, procSize);
    free(line);
    for (int i = 0; i < procSize; i++) {
	fclose(processes[i].readText);
	free(processes[i].name);
    }
    free(processes);
}

/* process_command()
 * -----------------
 * Checks the validity of the command given, if valid the command is executed
 * otherwise an error message is returned.
 *
 * arguments: 	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child 
 * 		processes.
 *
 * procSize: 	A pointer to an int with the number of child processes
 * 		spawned.
 *
 * Returns: 	updated processes with updated information.
 */
Process* process_command(Arguments arguments, Process* processes, 
	int* procSize) {
    // the command to be executed
    char* command = arguments.cmdArgs[0];

    // Check the command being run
    if (!strcmp(command, "spawn")) {
	processes = spawn_program(arguments, processes, procSize);
    } else if (!strcmp(command, "report")) {
	report_programs(arguments, processes, *procSize);
    } else if (!strcmp(command, "signal")) {
	send_signal(arguments, processes, *procSize);
    } else if (!strcmp(command, "sleep")) {
	sleep_command(arguments);	
    } else if (!strcmp(command, "send")) {
	send_text(arguments, processes, *procSize);
    } else if (!strcmp(command, "rcv")) {
	receive_text(arguments, processes, *procSize);
    } else if (!strcmp(command, "eof")) {
	eof(arguments, processes, *procSize);
    } else if (!strcmp(command, "cleanup")) {
	cleanup(processes, *procSize);
    } else {
	// error message
	printf("Error: Invalid command\n");
	fflush(stdout);
    }
    return processes;
}

/* spawn_program()
 * ---------------
 * Run <program> in a new process, optionally with arguments provided. 
 * The new process's stdin and stdout are connected to hq by pipes. Information
 * regarding the program is stored in processes then returned. Insufficent 
 * arguments will result in error message.
 *
 * arguments:	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child 
 * 		processes.
 *
 * procSize: 	A pointer to an int with the number of child processes
 * 		spawned.
 *
 * Returns:	updated processes with updated information.
 */
Process* spawn_program(Arguments arguments, Process* processes, 
	int* procSize) {
    if (arguments.argCount < 2) {
	printf("Error: Insufficient arguments\n"); // error message
	return processes;
    }
    // get the jobID
    int jobID = *procSize;
    // increase processes size to add new program
    processes = realloc(processes, sizeof(Process) * (jobID + 1));
    *procSize = *procSize + 1;

    // add processes name to processes
    processes[jobID].name = strdup(arguments.cmdArgs[1]);

    // creates 2 pipes for input and output to and from the program
    pipe(processes[jobID].fdRead);
    pipe(processes[jobID].fdWrite);

    // skip command
    arguments.cmdArgs++;

    printf("New Job ID [%d] created\n", jobID);

    // create child process
    processes[jobID].pid = fork();

    // child process
    if (!processes[jobID].pid) {
	// change stdin and stdout to pipes
	dup2(processes[jobID].fdRead[1], STDOUT_FILENO);
	dup2(processes[jobID].fdWrite[0], STDIN_FILENO);
	
	// close unused pipe ends of other processes
	for (int i = 0; i < *procSize; i++) {
	    close(processes[i].fdRead[0]);
	    close(processes[i].fdWrite[1]);
	}
	execvp(processes[jobID].name, arguments.cmdArgs);
	// if program fails to execute, exits with code 99
	exit(99);
    }
    // close unused pipe ends
    close(processes[jobID].fdRead[1]);
    close(processes[jobID].fdWrite[0]);

    // open file for reading the text child process
    processes[jobID].readText = fdopen(processes[jobID].fdRead[0], "r");

    return processes;
}

/* report_programs()
 * -----------------
 * Reports on the status of <jobid> or all jobs if no arguments are given. 
 *
 * arguments:	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child 
 * 		processes.
 * 		
 * procSize: 	An int with the number of child processes spawned.
 */
void report_programs(Arguments arguments, Process* processes, 
	int procSize) {

    int jobID = 0;
    // if a specific jobID is given execute the following
    if (arguments.argCount > 1) {
	if (valid_job(arguments.cmdArgs[1], procSize)) {
	    jobID = atoi(arguments.cmdArgs[1]);
	    printf("[Job] cmd:status\n");
	    print_status(processes, jobID);
	} else {
	    return;
	}
    } else { // if no jobID is given
	printf("[Job] cmd:status\n");
	while (jobID < procSize) {
	    print_status(processes, jobID);
	    jobID++;
	}
    }
}

/* print_status()
 * --------------
 * prints the status of jobID to stdout.
 *
 * processes: 	A list of struct Process with information regarding child
 * 		processes.
 *
 * jobID: 	The position of the job in processes
 */
void print_status(Process* processes, int jobID) {
    // basic information of process
    printf("[%d] %s:", jobID, processes[jobID].name);
    int status = 0;
    int state = 0;
    state = waitpid(processes[jobID].pid, &status, WNOHANG);
    // checks if waitpid has already been called on child process when it was
    // closed
    if (state == -1) { // if waitpid has already waited on closed program
	status = processes[jobID].status;
    } else {
	processes[jobID].status = status;
    }
    // prints the status of process
    if (state == 0) {
	printf("running\n");
    } else if (WIFEXITED(status)) {
	printf("exited(%d)\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
	printf("signalled(%d)\n", WTERMSIG(status));
    }
    fflush(stdout);
}

/* send_signal()
 * -------------
 * sends a signal to the specified job, prints error messages if 
 * invalid arguments are given. Prints error message if arguments are invalid.
 *
 * arguments:	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child
 * 		processes.
 *
 * procSize: 	An int with the number of child processes spawned.
 */
void send_signal(Arguments arguments, Process* processes, int procSize) {
    if (arguments.argCount < 3) {
	printf("Error: Insufficient arguments\n");
	return;
    }
    // check if job is invalid
    if (!valid_job(arguments.cmdArgs[1], procSize)) {
	return;
    }
    // check if signal is invalid
    if ((!is_number(arguments.cmdArgs[2])) ||
	    (atoi(arguments.cmdArgs[2]) > MAXIMUM_SIGNAL) ||
	    (atoi(arguments.cmdArgs[2]) < MINIMUM_SIGNAL)) {
	printf("Error: Invalid signal\n");
	return;
    }

    int jobID = atoi(arguments.cmdArgs[1]);
    int jobPid = processes[jobID].pid;
    int signal = atoi(arguments.cmdArgs[2]);
    
    // send signal to job
    kill(jobPid, signal);
}

/* sleep_command()
 * ---------------
 * sleeps for the number of seconds specified if valid. Error message
 * will be printed if invalid. Prints error message if arguments are invalid.
 *
 * arguments:	The arguments given by stdin to be processed.
 */
void sleep_command(Arguments arguments) {
    if (arguments.argCount < 2) {
	printf("Error: Insufficient arguments\n");
	return;
    }
    // checks if sleep time is a valid float number
    char* timeArg = arguments.cmdArgs[1];
    
    // gets rid of leading white spaces
    while (timeArg[0] == ' ') {
	timeArg++;
    }
    // check if empty string is given
    if (!strlen(timeArg)) {
	printf("Error: Invalid sleep time\n");
	return;
    }
    // checks how many decimals are in string
    int decimalCount = 0;
    // checks if string is a valid floating point number
    for (int i = 0; timeArg[i] != TERMINATOR; i++) {
	// increase the decimal count
	if (timeArg[i] == '.') {
	    decimalCount++;
	}
	// checks whether is digit or is decimal then checks if too 
	// many decimals are present
	if ((!isdigit(timeArg[i]) && (timeArg[i] != '.')) ||
		(decimalCount > 1)) {
	    printf("Error: Invalid sleep time\n");
	    return;
	}
    }
    // convert to floating point number
    float time = strtod(arguments.cmdArgs[1], NULL);
    // checks if the sleep time is a negative number
    if (time < 0) {
	printf("Error: Invalid sleep time\n");
	return;
    }
    // convert seconds to microseconds
    time *= MICROSECONDS;
    
    usleep(time);
}

/* send_text()
 * -----------
 * sends text to the specified job. Error messaged will be printed if commands
 * are invalid. Prints error message if arguments are invalid.
 *
 * arguments:	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child
 * 		processes.
 *
 * procSize: 	An int with the number of child processes spawned.
 */
void send_text(Arguments arguments, Process* processes, int procSize) {
    // checks if job is valid only if there are enough arguments present
    if ((arguments.argCount > 1) && 
	    !valid_job(arguments.cmdArgs[1], procSize)) {
	return;
    }
    // checks if number of arguments are present
    if (arguments.argCount < 3) {
	printf("Error: Insufficient arguments\n");
	return;
    }

    int jobID = atoi(arguments.cmdArgs[1]);
    int fdWrite = processes[jobID].fdWrite[1];

    // add newline to end of string
    char newLine = NEWLINE;
    strncat(arguments.cmdArgs[2], &newLine, 1);

    // write() to pipe fdWrite
    write(fdWrite, arguments.cmdArgs[2], strlen(arguments.cmdArgs[2]));
}

/* receive_text()
 * --------------
 * Attempts to read one line of text from the specified jobs stdout and display
 * it to hq stdout. Prints error message if arguments are invalid.
 *
 * arguments:	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child
 * 		processes.
 *
 * procSize: 	An int with the number of child processes spawned.
 */
void receive_text(Arguments arguments, Process* processes, int procSize) {
    // checks if job is valid only if there enought arguments present
    if ((arguments.argCount > 1) &&
	    !valid_job(arguments.cmdArgs[1], procSize)) {
	return;
    }
    // checks if number of arguments are present
    if (arguments.argCount < 2) {
	printf("Error: Insufficient arguments\n");
	return;
    }

    int jobID = atoi(arguments.cmdArgs[1]);
    int fdRead = processes[jobID].fdRead[0];
    FILE* file;

    // if fd doesn't block read from file
    if (is_ready(fdRead)) {
	file = processes[jobID].readText;
	char* line = read_line(file);
	// check if EOF is detected
	if (line == NULL) {
	    printf("<EOF>\n");
	} else {
	    printf("%s\n", line);
	}
	free(line);
    } else {
	printf("<no input>\n");
    }
    fflush(stdout);
}

/* eof()
 * -----
 * close the pipe connected to the specified job's stdin, thus causing that 
 * job to receive EOF on its next read attempt. Prints error message
 * if insufficient arguments are present.
 *
 * arguments:	The arguments given by stdin to be processed.
 *
 * processes: 	A list of struct Process with information regarding child
 * 		processes.
 *
 * jobID: 	The position of the job in processes
 * procSize: 	An int with the number of child processes spawned.
 */
void eof(Arguments arguments, Process* processes, int procSize) {
    // checks if there are sufficient arguments present
    if (arguments.argCount < 2) {
	printf("Error: Insufficient arguments\n");
	return;
    }
    // check if jobID is valid
    if (!valid_job(arguments.cmdArgs[1], procSize)) {
	return;
    }

    int jobID = atoi(arguments.cmdArgs[1]);
    int fdWrite = processes[jobID].fdWrite[1];

    close(fdWrite);
}

/* cleanup()
 * ---------
 * Terminate and reap all child processes spawned by hq by sending them
 * SIGKILL (signal 9).
 *
 * processes: 	A list of struct Process with information regarding child
 * 		processes.
 *
 * procSize: 	An int with the number of child processes spawned.
 */
void cleanup(Process* processes, int procSize) {
    int status;
    for (int jobID = 0; jobID < procSize; jobID++) {
	// sends SIGKILL to process
	pid_t jobPid = processes[jobID].pid;
	kill(jobPid, SIGKILL);
	// reaps children and updates status if program hasn't previously been
	// reaped on exit
	if (waitpid(processes[jobID].pid, &status, 0) != -1) {
	    processes[jobID].status = status;
	}
    }
}

/* valid_job()
 * -----------
 * Checks wether the string is a valid jobID
 *
 * procSize: 	An int with the number of child processes spawned.
 *
 * jobID: 	The position of the job in processes
 */
bool valid_job(char* jobID, int procSize) {
    // checks if string is empty
    if (!strlen(jobID)) {
	printf("Error: Invalid job\n");
	return false;
    }
    if ((!is_number(jobID)) ||
	    (atoi(jobID) >= procSize) ||
	    (atoi(jobID) < 0)) {
	printf("Error: Invalid job\n");
	return false;
    }
    return true;
}

/* is_number()
 * ----------
 * checks if the string contains only numbers
 *
 * number: The string with madeup of only numbers
 */
bool is_number(char* number) {
    // parse through leading white space
    while (number[0] == ' ') {
	number++;
    }
    // checks if string is empty
    if (!strlen(number)) {
	return false;
    }
    for (int i = 0; number[i] != TERMINATOR; i++) {
	if (!isdigit(number[i])) {
	    return false;
	}
    }
    return true;
}

/* setup_sig_handler()
 * -------------------
 *  Sets up the signal handler for SIGINT (signal 2) where it just ignores it.
 */
void setup_sig_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags = SA_RESTART;

    // sets up action to be taken when signal is called
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGPIPE, &sa, 0);
}

/* sig_handler()
 * -------------
 * signal handler which does nothing
 */
void sig_handler(int sigNum) {
}
