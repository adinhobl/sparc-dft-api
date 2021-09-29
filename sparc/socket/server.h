#include <stdio.h>
#include <sys/signal.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <assert.h>

/*
 * max length for a request message, currently stack allocated,
 * but ideally would be heap allocated and flexible. then this 
 * would solely by the amount that a single recv could get, and
 * not the full message length
 */
#define MAX_REQUEST_LEN 8192 

/*
 * Defines status codes for common statuses and errors in server
 * client communications. 
 */
typedef int serverstatus_t;

#define  OK 200
#define  ERROR 500
#define  INVALID 600

/*
 * Struct that is passed throughout the code with common
 * information about the socket connection session
 * 
 * sockfd: the file descriptor for the current socket
 * STATUS: tracks the status of the current session. At
 *         many places throughout the code, this is checked
 * request_type: stores the type of interaction after parsing
 *         the client request. This determines which handling 
 *         functions are called later.
 * request_len: the length of the initial message passed by
 *         the client, not including the beginning signature
 *         or ending signature
 */
typedef struct connectinfo_t{
    int sockfd;
    serverstatus_t STATUS;
    int request_type;
    int request_len;
} connectinfo_t;


/* 
 * Struct that handles server setup information not related to 
 * the details of client-server communication.
 * 
 * port: inet port number for connection to the server
 * max_queue: the maximum number of waiting connections that
 *          the server will allow. Others will be rejected.
 * hints: holds details for server setup and connection
 */
typedef struct server_t {
    char port[6];
    int max_queue; 
    struct addrinfo hints;

} server_t;

/* 
 * Creates a server by populating the server struct. Connections
 * using this information are initialized later in server_serve().
 * 
 * Arguments:
 *  None
 */
server_t *server_create();

/*
 * Sets the port where the server will listen for client connections.
 * 
 * Arguments:
 *  serv: pointer to server_t object from server_create() where the 
 *          port information will be populated.
 *  port: number of the port for the server to connect to. Must be 
 *          less than 65535.
 */
void server_set_port(server_t **serv, unsigned short port);

/*
 * Sets the maximum number of waiting connections that
 * the server will allow. Others will be rejected. The server
 * operates in a single-threaded manner, so one request will
 * be fully completed before the server handles another one.
 * 
 * Arguments:
 *  serv: pointer to server_t object from server_create() where the 
 *          port information will be populated.
 *  max_queue: the maximum number of connections. Good numbers are in 
 *          the single or low double digits.
 */
void server_set_maxqueue(server_t **serv, int max_queue);

/*
 * Begins the server loop. Roughly, this corresponds to:
 *  binding the server to a socket
 *  And then, in a loop:
 *      listening and accepting a client connection
 *      receiving the client's initial request message
 *      parsing the request to find what handling functions to call
 *      dispatching the message payload to the appropraite function
 *      freeing various variable and closing the connection to the client
 * 
 * This loop does not (yet terminate) but hoopefully will
 * in the future with client calls.
 *
 * Arguments:
 *  serv: pointer to server_t object from server_create() where the 
 *          port information will be populated.
 */
void server_serve(server_t **serv);


/*****                                       *****
 *****    Internal server function calls     *****
 *****                                       *****/ 

/*
 * Sends a message at data of length size to the client based
 * on connection info from cnxinfo. Is superior to a regular 
 * send call because it will handle incomplete sends of single
 * messages and ensure that the full message is sent.
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data. 
 *  data: pointer to the beginning of the message that needs to 
 *          be sent. data can be characters or other data formats
 *  size: the size of the message at data that needs to be sent, 
 *          in bytes
 */
ssize_t serv_int_send(connectinfo_t **cnxinfo, const void *data, size_t size);

/*
 * Binds the server with parameters specified by serv to 
 * the socket s. 
 * 
 * Arguments:
 *  serv: pointer to server_t object from server_create() 
 *  s: socket number to bind the server
 */
void serv_int_bind_socket(server_t **serv, int *s);

/*
 * Listens and accepts client connections. Returns a file
 * descriptor for the connections session for further 
 * communications.
 * 
 * Arguments:
 *  serv: pointer to server_t object from server_create() 
 *  s: socket number to bind the server
 */
int serv_int_listen_accept(server_t **serv, int *s);

/* 
 * Parses the client message to determine where to route the message
 * payload to.
 * Check the first characters to determine what type of request it is. 
 * Currently defined message headers are:
 *      1 - "ASE CALC " 
 *      2 - "ECHO "
 * Returns allocated memory for later usage by the handling functions.
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 *  req_buf: the message from the client including the header that
 *          specifies which handling function to route to
 */
char *serv_int_parse_request(connectinfo_t **cnxinfo, char req_buf[]);

/*
 * UNUSED
 * 
 * Sends the client a header message about the status of its request.
 */
// ssize_t serv_int_sendheader(connectinfo_t **cnxinfo, serverstatus_t status, size_t file_len);


/*****                                       *****
 *****    Protocol functions for messages    *****
 *****                                       *****/ 

/*
 * Echos the request data back to the client. 
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 *  request: data that will be sent directly back to the client
 */
int protocol_echo(connectinfo_t **cnxinfo, char *request);

/*
 * Calls the SPARC calculating function on the request data. 
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 *  request: data that will be sent to the SPARC calculating function
 */
int protocol_calc(connectinfo_t **cnxinfo, char *request);

/*
 * Dispatches client messages to the appropriate handler function 
 * based on the parsed starting sequence.  
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so the 
 *          handling functions can send its data.
 *  request: the data that will be routed to the handling functions
 */
int protocol_dispatch(connectinfo_t **cnxinfo, char *request);

/*
 * UNUSED
 * 
 * Handler function to abort the current session and shut down the server. 
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 */
void protocol_abort(connectinfo_t **cnxinfo);


/*****                                       *****
 ***** Old code or code for future adaptions *****
 *****                                       *****/                                      

// typedef size_t handler_error_t;

// void server_set_handler(server_t **serv, handler_error_t (*handler)(connectinfo_t **, const char *, void*));

// void server_set_handlerarg(server_t **serv, void* arg);

// handler_error_t serv_int_handler(connectinfo_t **cnxinfo, const char *path, void* arg);

// handler_error_t (*handler)(); // would go in server struct
// void *harg; // would go in server struct

