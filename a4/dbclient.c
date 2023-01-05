/*
** dbclient.c
**	CSSE2310 - Assignment three - 2022 - Semester One
**	
**	Written by Erik Flink
**
** Usage:
**	dbclient portnum key [value]
** The portnum argument indicates which localhost port dbserver is listening on
** - either numerical or the name of the service
** The key value argument specifies the key to be read or written
** The value argument, if provided, specifies the value to be written to the
** database for the corresponding key. If value is not provided then dbclient 
** will read the value from the database
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <csse2310a4.h>

// minimum commandline arguments
#define MINARGUMENTS 2

// newline character
#define NEWLINE '\n'

// string terminator
#define TERMINATOR '\0'

// Enumerated type with exit types
typedef enum {
    INSUFFICIENT,
    INVALID_KEY,
    CONNECTION_ERROR
} ErrorType;

// HTTP request argument given by commandline
typedef struct {
    char* type;
    char* portNum;
    char* key;
    char* value;
} Request;

/* Function prototypes - see descriptions with the functions themselves */
void send_request(Request request, FILE* to);
int receive_request(FILE* from, char* type);
int get_socket(char* portNum);
Request process_commandline(int argc, char** argv);
void exit_program(ErrorType error);

/*****************************************************************************/
int main(int argc, char** argv) {

    // process commandline and place arguments into request.
    Request request;
    request = process_commandline(argc, argv);

    // connects to port and returns file descriptor
    int fd;
    fd = get_socket(request.portNum);

    // turns file descriptors into separate read and write files
    int fd2 = dup(fd);
    FILE* to = fdopen(fd, "w");
    FILE* from = fdopen(fd2, "r");

    // send http request to FILE*
    send_request(request, to);

    // receive http response and return the exitStatus
    return receive_request(from, request.type);
}

/* send_request()
 * --------------
 * Sends Http request to file descriptor, this request is dependent on 
 * values in Request struct.
 */
void send_request(Request request, FILE* to) {
    // send HTTP request to server according to Request struct.
    if (!strcmp(request.type, "PUT")) {
	fprintf(to, "PUT /public/%s HTTP/1.1\r\nContent-Length: %ld\r\n\r\n%s",
		request.key, strlen(request.value), request.value);
    } else {
	fprintf(to, "GET /public/%s HTTP/1.1\r\n\r\n", request.key);
    }
    fflush(to);
    fclose(to);
}

/* receive_request()
 * -----------------
 *  Receive response from server to verify the validity of request, if type is
 *  get, prints the key value. Function then returns an exit number 
 *  according to the response.
 */
int receive_request(FILE* from, char* type) {
    // change exitStatus depending on request type
    int exitStatus;
    if (!strcmp(type, "PUT")) {
	exitStatus = 4;
    } else {
	exitStatus = 3;
    }

    // variables to hold information when calling get_HTTP_response.
    int status;
    char* statusExplanation;
    char* body;
    HttpHeader** headers;

    if (!get_HTTP_response(from, &status, &statusExplanation, &headers, 
	    &body)) {
	// Got EOF or badly formed response
	return exitStatus;
    } 
    // check if server doesn't respond with OK response
    if (status != 200) {
	return exitStatus;
    }
    // print key value if of GET request was sent
    if (!strcmp(type, "GET")) {
	printf("%s\n", body);
    }
    free_array_of_headers(headers);
    free(statusExplanation);
    free(body);
    fclose(from);
    return 0;
}

/* get_socket()
 * ------------
 * Connects to the portnumber provided and returns the file descriptor 
 * associated once connected. If invalid port then error message is printed
 * then program exits.
 */
int get_socket(char* portNum) {
    const char* port = portNum;
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int err;
    // workout address, if to unable print error message and exit.
    if ((err = getaddrinfo("localhost", port, &hints, &ai))) {
	freeaddrinfo(ai);
	exit_program(CONNECTION_ERROR);
    }

    // connect to port, if unable to print error message and exit.
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(fd, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
	exit_program(CONNECTION_ERROR);
    }
    freeaddrinfo(ai);
    return fd;
}

/* process_commandline()
 * ---------------------
 * Goes through the command line arguments and checks their validity.
 * If the command line is invalid, then we print a usage error message and 
 * exit.
 */
Request process_commandline(int argc, char** argv) {
    // Skip over the program name argument.
    argc--;
    argv++;

    Request request;

    // Checks if sufficient commandline arguments are given.
    if (argc < MINARGUMENTS) {
	exit_program(INSUFFICIENT);
    }

    // Checks if [value] is specified in commandline then sets request type.
    if (argc > MINARGUMENTS) {
	request.type = "PUT";
    } else {
	request.type = "GET";
    }

    // Sets portNum
    request.portNum = argv[0];

    // Checks if key is valid - doesn't contain any spaces or newline 
    // characters.
    char* key = argv[1];
    for (int i = 0; key[i] != TERMINATOR; i++) {
	if ((key[i] == NEWLINE) || (key[i] == ' ')) {
	    exit_program(INVALID_KEY);
	}
    }
    request.key = key;

    // Sets value if request is of type PUT
    if (!strcmp(request.type, "PUT")) {
	request.value = argv[2];
    }

    return request;
}

/* exit_program()
 * --------------
 * Prints error message and exits corresponding to the ErrorType.
 */
void exit_program(ErrorType error) {
    switch (error) {
	case (INSUFFICIENT):
	    fprintf(stderr, "Usage: dbclient portnum key [value]\n");
	    exit(1);
	case (INVALID_KEY):
	    fprintf(stderr, 
		    "dbclient: key must not contain spaces or newlines\n");
	    exit(1);
	    break;
	case (CONNECTION_ERROR):
	    fprintf(stderr, "dbclient: Unable to connect to port N\n");
	    exit(2);
    }
}
