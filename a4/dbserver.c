/*
** dbserver.c
** 	CSSE2310 - Assignement four - 2022 - Semester Oe
**	
**	Written by Erik Flink
**
** usage:
**	dbserver authfile connections [portnum]
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csse2310a3.h>
#include <csse2310a4.h>
#include <ctype.h>
#include <pthread.h>
#include <stringstore.h>
#include <semaphore.h>
#include <signal.h>

// minimum commandline arguments
#define MINARGUMENTS 2

// maximum commandline arguments
#define MAXARGUMENTS 3

// Minimum valid port number.
#define MINPORTNUMBER 1024

// Maximum valid port number.
#define MAXPORTNUMBER 65535

// Enumerated type holding Error types
typedef enum {
    INVALID_COMMANDLINE,
    INVALID_AUTH,
    INVALID_PORT
} ErrorType;

// Enumerated type holding HTTP response types
typedef enum {
    OK = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    NOT_FOUND = 404,
    INTERNAL_ERROR = 500,
    SERVICE_UNAVAILABLE = 503
} Response;

// Structure type holding HTTP request information.
typedef struct {
    char* method;
    char* address;
    char* body;
    HttpHeader** headers;
    char* privacy;
    char* key;
} Request;

// Structure type holding information reflecting programs operations.
typedef struct {
    int connected;
    int completed;
    int authFail;
    int get;
    int put;
    int delete;
} Stats;

// Structure type holding server information.
typedef struct {
    char* auth;
    int connections;
    int fd;
    sigset_t signals;
    sem_t stringLock;
    StringStore* publicStore;
    StringStore* privateStore;
    Stats stats;
} Server;

// Structure holding client parameters.
typedef struct {
    int fd;
    Server* server;
} Client;

/* Function prototypes - see descriptions with the functions themselves */
void process_connections(Server server);
void* limit_thread(void* arg);
void* client_thread(void* arg);
void* signal_thread(void* arg);
void initialize_server(Server* server);
bool process_http_request(FILE* to, FILE* from, Server* server);
void process_request_arguments(FILE* to, Request request, Server* server);
void send_http_response(FILE* to, Response response, char* value);
Server process_commandline(int argc, char* argv[]);
int open_listen(const char* port, int connections);
char* authenticate(char* authFile);
void exit_program(ErrorType error);
bool is_number(char* number);

/*****************************************************************************/
int main(int argc, char* argv[]) {
    Server server;
    server = process_commandline(argc, argv);
    process_connections(server);
    return 0;
}

/* process_connections()
 * ---------------------
 * Repeatedly accepts connections, a thread is spawned for each incoming
 * connection.
 */
void process_connections(Server server) {
    // Initiates server information variables
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    initialize_server(&server);

    // Repeatedly accept connections
    while (true) {
	fromAddrSize = sizeof(struct sockaddr_in);
	fd = accept(server.fd, (struct sockaddr*)&fromAddr, &fromAddrSize);
	pthread_t threadId;

	// Checks if connection limit is reached
	if ((server.stats.connected >= server.connections) 
		&& (server.connections != 0)) {
	    pthread_create(&threadId, NULL, limit_thread, &fd);
	    pthread_detach(threadId);
	    continue;
	}
	// Checks if theres an error connecting to server
	if (fd < 0) {
	    continue;
	}
	// Updates servers connected stat
	server.stats.connected++;

	// Creates client
	Client* client = malloc(sizeof(Client));
	client->server = &server;
	client->fd = fd;

	// Creates and detatches thread
	pthread_create(&threadId, NULL, client_thread, client);
	pthread_detach(threadId);
    }
}

/* initialize_server()
 * ------------------
 * Intitializes server structure to initialise signal handler and set values
 * in struct before connecting to clients.
 */
void initialize_server(Server* server) {

    // Creates thread to handle SIGHUP signal
    sigemptyset(&server->signals);
    sigaddset(&server->signals, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &server->signals, NULL);
    pthread_t threadSigId;
    pthread_create(&threadSigId, NULL, signal_thread, server);

    // Place key-value stores into server struct.
    server->publicStore = stringstore_init();
    server->privateStore = stringstore_init();

    // Sets all server stats to 0
    memset(&server->stats, 0, sizeof(Stats));

    // Initiate semaphore lock
    sem_init(&server->stringLock, 0, 1);
}

/* limit_thread()
 * --------------
 * Handles connections that are over the connection limit, sending a
 * Service unavailable response then closing connection.
 */
void* limit_thread(void* arg) {
    int fd = *(int*) arg;
    FILE* to = fdopen(fd, "w");
    send_http_response(to, SERVICE_UNAVAILABLE, NULL);
    fclose(to);
    return NULL;
}

/* signal_thread()
 * ---------------
 * Upon receiving SIGHUP signal prints server operations statistics reflecting
 * the programs up to date operations.
 */
void* signal_thread(void* arg) {
    Server* server = (Server*)arg;
    Stats* stats = &server->stats;
    int sig;

    while (true) {
	// Waits until SIGHUP signal is received.
	sigwait(&server->signals, &sig);

	sem_wait(&server->stringLock);

	// Prints all operation statistics
	fprintf(stderr, "Connected clients:%d\n", stats->connected);
	fprintf(stderr, "Completed clients:%d\n", stats->completed);
	fprintf(stderr, "Auth failures:%d\n", stats-> authFail);
	fprintf(stderr, "GET operations:%d\n", stats->get);
	fprintf(stderr, "PUT operations:%d\n", stats->put);
	fprintf(stderr, "DELETE operations:%d\n", stats->delete);
	fflush(stderr);

	sem_post(&server->stringLock);
    }
    return NULL;
}

/* client_thread()
 * ---------------
 * A client handler thread that loops waiting for a HTTP request. If an
 * invalid request is recieved, client is disconnected. If valid, updates
 * or sends key-value store to client and sends response.
 */
void* client_thread(void* arg) {
    Client client = *(Client*)arg;
    free(arg);
    Server* server = client.server;
    
    int fd2 = (dup(client.fd));
    FILE* to = fdopen(client.fd, "w");
    FILE* from = fdopen(fd2, "r");

    // Loops and processes new requests from client
    while (true) {
	if (!process_http_request(to, from, server)) {
	    break;
	}
    }
    // Updates server stats
    server->stats.connected--;
    server->stats.completed++;

    fclose(to);
    fclose(from);
    return NULL;
}

/* process_http_request()
 * -------------------
 * Reads HTTP request from file stream then processes the information
 * updating or retrieving the key-value store and sending a 
 * response back to client. Returns true if sucessful, otherwise false.
 */
bool process_http_request(FILE* to, FILE* from, Server* server) {

    // Information regarding http request type
    Request request;

    if (!get_HTTP_request(from, &request.method, &request.address, 
	    &request.headers, &request.body)) {
	return false;
    }

    // Split address into usable bits of information
    char** addresses = split_by_char(request.address, '/', 3);
    char* empty = addresses[0];
    request.privacy = addresses[1];
    request.key = addresses[2];

    // Check if address is incorrect
    if (strcmp(empty, "") || (strcmp(request.privacy, "private") 
	    && strcmp(request.privacy, "public"))) {
	send_http_response(to, BAD_REQUEST, NULL);
	return true;
    }

    // Processes arguments, only one client is allowed to edit StringStore
    // structs at a time.
    sem_wait(&server->stringLock);
    process_request_arguments(to, request, server);
    sem_post(&server->stringLock);
    
    return true;
}

/* process_request_arguments()
 * ---------------------------
 * Processes the arguments given by the HTTP request. Updates/retrieves
 * key-value stores, sends a response back to the client and updates
 * server stats.
 */
void process_request_arguments(FILE* to, Request request, Server* server) {
    StringStore* stringStore = server->publicStore;

    // Changes stringStore if request is private and valid.
    if (!strcmp(request.privacy, "private")) {
	if (request.headers[0] == NULL) { // Checks if authString not present.
	    send_http_response(to, UNAUTHORIZED, NULL);
	    server->stats.authFail++;
	    return;
	}
	char* guess = request.headers[0]->value; // authentication string.
	if (strcmp(guess, server->auth)) { // Check if guess is authString
	    send_http_response(to, UNAUTHORIZED, NULL);
	    server->stats.authFail++;
	    return;
	}
	stringStore = server->privateStore;
    }
    if (!strcmp(request.method, "PUT")) { 
	// Tries to store key value
	if (stringstore_add(stringStore, request.key, request.body)) {
	    // Success
	    send_http_response(to, OK, NULL);
	    server->stats.put++;
	} else {
	    send_http_response(to, INTERNAL_ERROR, NULL);
	}
    } else if (!strcmp(request.method, "GET")) {
	char* rec = (char*) stringstore_retrieve(stringStore, request.key);
	// Checks if key-value pair is present then sends response.
	if (rec != NULL) {
	    // Success
	    send_http_response(to, OK, rec);
	    server->stats.get++;
	} else {
	    send_http_response(to, NOT_FOUND, NULL);
	}
    } else if (!strcmp(request.method, "DELETE")) {
	if (stringstore_delete(stringStore, request.key)) {
	    // Successfully deleted
	    send_http_response(to, OK, NULL);
	    server->stats.delete++;
	} else {
	    send_http_response(to, NOT_FOUND, NULL);
	}
    } else {
	// Invalid method provided
	send_http_response(to, BAD_REQUEST, NULL);
    }
    free_array_of_headers(request.headers);
}

/* send_http_response()
 * -------------------
 * Sends a HTTP response to file stream according to the reponse given.
 * Sends a HTTP response to file stream. Response is constructed based on 
 * response type and if value argument is not NULL.
 */
void send_http_response(FILE* to, Response response, char* value) {

    // default information used for HTTP response.
    int status;
    char* statusExplain;
    char* body;
    body = "";
    HttpHeader** headers = calloc(2, sizeof(HttpHeader*));
    HttpHeader* header = calloc(1, sizeof(HttpHeader));
    headers[0] = header;
    headers[0]->name = "Content-Length";
    headers[0]->value = "0";

    // Updates or adds to default information depending on response.
    switch (response) {
	case (OK):
	    status = OK;
	    statusExplain = "OK";
	    if (value != NULL) {
		headers[0]->value = (char*) strlen(value);
		body = value;
	    }
	    break;
	case (BAD_REQUEST):
	    status = BAD_REQUEST;
	    statusExplain = "Bad Request";
	    break;
	case (NOT_FOUND):
	    status = NOT_FOUND;
	    statusExplain = "Not Found";
	    break;
	case (INTERNAL_ERROR):
	    status = INTERNAL_ERROR;
	    statusExplain = "Internal Server Error";
	    break;
	case (UNAUTHORIZED):
	    status = UNAUTHORIZED;
	    statusExplain = "Unauthorized";
	    break;
	case (SERVICE_UNAVAILABLE):
	    status = SERVICE_UNAVAILABLE;
	    statusExplain = "Service Unavailable";
    }
    // Constructs and sends response to client.
    char* httpResponse;
    httpResponse = construct_HTTP_response(status, statusExplain,
	    headers, body);
    fprintf(to, "%s", httpResponse);
    fflush(to);
}

/* process_commandline()
 * ---------------------
 * Goes through the command line arguments and checks their validity. If the 
 * command line is invalid, then we print a usage error message and exit.
 */
Server process_commandline(int argc, char* argv[]) {
    // skip over the program name argument
    argc--;
    argv++;
    
    Server server;

    // Checks if number of commandline arguments are overabundant 
    // or insufficent.
    if ((argc < MINARGUMENTS) || (argc > MAXARGUMENTS)) {
	exit_program(INVALID_COMMANDLINE);
    }

    char* authFile = argv[0];
    char* connections = argv[1];

    // Checks if connection argument is a non-negative integer
    if (!is_number(connections) || (atoi(connections) < 0)) {
	exit_program(INVALID_COMMANDLINE);
    }
    server.connections = atoi(connections);

    // Checks portnum argument (if present) is not an integer value, or a
    // valid port number.
    char* port;
    if (argc == MAXARGUMENTS) {
	port = argv[2];
	// Checks if portNum is a number.
	if (is_number(port)) {
	    int portNum = atoi(port);
	    // Check if portNum is a valid port number.
	    if ((portNum != 0 && portNum < MINPORTNUMBER) 
		    || portNum > MAXPORTNUMBER) {
		exit_program(INVALID_COMMANDLINE);
	    }
	} else {
	    // portnumber wasn't an integer.
	    exit_program(INVALID_COMMANDLINE);
	}
    } else {
	// if no portnumber given set to 0.
	port = "0";
    }
    // Open authfile and set as authentication string
    server.auth = authenticate(authFile);

    // Listens
    server.fd = open_listen(port, atoi(connections));

    return server;
}

/* open_listen()
 * -------------
 * Listens on a given port. Returns listening socket or prints error message
 * and exits on failure.
 */
int open_listen(const char* port, int connections) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;		// IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 	// listen on all IP addresses

    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &ai))) {
	// Could not determine the address
	freeaddrinfo(ai);
	exit_program(INVALID_PORT);
    }

    // Create a socket and bind it to a port
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); 

    // Allow address (port number to be reusted immediately
    int optVal = 1;
    if (setsockopt(listenfd, SOL_SOCKET, 
	    SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
	// Error setting socket option
	exit_program(INVALID_PORT);
    }
    if (bind(listenfd, (struct sockaddr*)ai->ai_addr, 
	    sizeof(struct sockaddr)) < 0) {
	// Error binding
	exit_program(INVALID_PORT);
    }

    if (listen(listenfd, connections) < 0) {
	exit_program(INVALID_PORT);
    }

    // Check what port we are listening on
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(listenfd, (struct sockaddr*)&ad, &len)) {
	exit_program(INVALID_PORT);
    }
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));

    return listenfd;
}

/* authenticate()
 * -------------
 * Opens authfile and sets the authentication string to the first line of file.
 */
char* authenticate(char* authFile) {
    FILE* auth = fopen(authFile, "r");
    // File cannot be opened
    if (!auth) {
	// error
	exit_program(INVALID_AUTH);
    }
    // Reads line and checks if no line is present.
    char* authentication = read_line(auth);
    if (authentication == NULL) {
	exit_program(INVALID_AUTH);
    }
    return authentication;
}

/* exit_program()
 * --------------
 * Prints an error message and exits corresponding to the ErrorType.
 */
void exit_program(ErrorType error) {
    switch (error) {
	case (INVALID_COMMANDLINE):
	    fprintf(stderr, 
		    "Usage: dbserver authfile connections [portnum]\n");
	    exit(1);
	    break;
	case (INVALID_AUTH):
	    fprintf(stderr, 
		    "dbserver: unable to read authentication string\n");
	    exit(2);
	    break;
	case (INVALID_PORT):
	    fprintf(stderr, "dbserver: unable to open socket for listening\n");
	    exit(3);
	    break;
    }
}

/* is_number()
 * -----------
 *  Checks if the string contains only numbers.
 *
 *  number: The string made up of only numbers.
 */
bool is_number(char* number) {
    // parse through leading white space
    while (number[0] == ' ') {
	number++;
    }
    // Checks if string is empty
    if (!strlen(number)) {
	return false;
    }
    for (int i = 0; number[i] != '\0'; i++) {
	if (!isdigit(number[i])) {
	    return false;
	}
    }
    return true;
}
