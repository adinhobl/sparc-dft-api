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
#define MAX_REQUEST_LEN 128 

/*
 * Defines status codes for common statuses and errors in server
 * client communications. 
 */
typedef int serverstatus_t;

#define  OK 200
#define  ERROR 500
#define  INVALID 600


/* Used in routing appropriate interaction functions */
#define ABORT -1
#define STATUS 1 
#define INIT 2
#define POSDATA 3
#define GETFORCE 4
#define GETSTRESS 5
#define ECHO 99

/*
 * Struct that is passed throughout the code with common
 * information about the socket connection session
 * 
 * sockfd: the file descriptor for the current socket
 * cnx_status: tracks the status of the current session. At
 *         many places throughout the code, this is checked
 * request_type: stores the type of interaction after parsing
 *         the client request. This determines which handling 
 *         functions are called later.
 */
typedef struct connectinfo_t{
    int sockfd;
    serverstatus_t cnx_status;
    int request_type;
} connectinfo_t;


/*
 * Struct that tracks information on the current state of 
 * the calculation.
 * 
 * sockfd: the file descriptor for the current socket
 * cnx_status: tracks the status of the current session. At
 *         many places throughout the code, this is checked
 * request_type: stores the type of interaction after parsing
 *         the client request. This determines which handling 
 *         functions are called later.
 */
typedef struct calc_state_t{
    int32_t bead_index;
    int32_t init_string_len;
    char *init_string;
    double cell_matrix[9];
    double inv_matrix[9];
    int32_t num_atoms;
    double *atom_coords; 
    double potential;
    double virial[9];  
    // JSON string len
    // JSON String
} calc_state_t;


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

/*****                                       *****
 *****             Server setup              *****
 *****                                       *****/ 

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
 * Binds the server to a socket. Then listens for and accepts a client 
 * connection. Creates a connectinfo struct to pass to different
 * functions so they can communicate with the client.
 *  
 * Arguments:
 *  serv: pointer to server_t object from server_create() where the 
 *          port information will be populated.
 *  s: file descriptor for the socket communications
 */
connectinfo_t *server_connect(server_t **serv, int *s);

/*
 * Begins the server loop to process client queries. Roughly, this
 * corresponds to:
 *   receiving the client's initial request message
 *   parsing the request to find what handling functions to call
 *   dispatching the message payload to the appropriate function
 * 
 * This loop does not (yet terminate) but hoopefully will
 * in the future with client calls.
 */ 
void server_process(connectinfo_t *cnxinfo);

/* 
 * Cleans up current session by closing sockets, freeing ports, freeing
 * connection context, and shuts down the server. 
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 *  serv: pointer to server_t object from server_create() where the 
 *          port information will be populated.
 *  s: file descriptor for the socket communications
 * 
 * Returns: status code (right now, just 0)
 */
int server_abort(connectinfo_t *cnx, server_t *serv, int s);


/*****                                       *****
 *****    Internal server function calls     *****
 *****                                       *****/ 

/*
 * Sends a buffer of data of length size to the client based
 * on connection info from cnxinfo. Is superior to a regular 
 * send call because it will handle incomplete sends of single
 * messages and ensure that the full message is sent.
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data. 
 *  data: pointer to the beginning of the message that needs to 
 *          be sent. data can be characters or other data formats
 *  len: the size of the message at data that needs to be sent, 
 *          in bytes
 */
ssize_t serv_int_send(connectinfo_t *cnxinfo, const void *data, size_t len);

/*
 * Recieves a buffer of data of unknown size (max size MAX_REQUEST_LEN)
 * from the client based on connection info from cnxinfo. Is superior
 * to a regular send call because it will handle incomplete recvs of single
 * messages and ensure that the full message is sent. To do this, it scans
 * for the End-Of-Response sentinel characters, eor_sig. req_buf will be 
 * cleared at the beginning of this function.
 * 
 * NOTE: for messages longer than MAX_REQUEST_LEN, you will want to allocate
 *       memory instead of using this.
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data. 
 *  req_buf: a buffer of max size MAX_REQUEST_LEN that is used to hold
 *          the short messages for this protocol
 */
ssize_t serv_int_recv(connectinfo_t *cnxinfo, char req_buf[]);

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
 *      1 - "STATUS" 
 *      2 - "INIT"
 *      3 - "POSDATA"
 *      4 - "GETFORCE"
 *      5 - "ABORT"
 *     99 - "ECHO"
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 *  req_buf: the message from the client including the header that
 *          specifies which handling function to route to
 */
void serv_int_parse_request(connectinfo_t *cnxinfo, char req_buf[]);

/*
 * Dispatches client messages to the appropriate handler function 
 * based on the parsed starting sequence.  
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so the 
 *          handling functions can send its data.
 *  calc_state: a pointer to a calc_state_t object with fields used to
 *          initialize and track the state of a calculation.
 */
int serv_int_dispatch(connectinfo_t *cnxinfo, calc_state_t *calc_state);

/* 
 * Initializes the calculation state that will be reused for all the
 * calculations. This state will be updated based on server-client  
 * communications and the results of any calculation. Maintaining the 
 * state allows for more granular and dynamic update and query calls.
 * Most fields are initialized with zeros or left uninitialized. Whether
 * state has changed from its initialized values partially determines
 * the server's response to client queries.
 * 
 * Arguments:
 *  calc_state: a pointer to a calc_state_t object with fields used to
 *          initialize and track the state of a calculation.
 * 
 */
void serv_int_init_calc_state(calc_state_t *calc_state);

/* 
 * Cleans up the calculation state. Frees any memory with a pointer in
 * the calc_state object.
 * 
 * Arguments:
 *  calc_state: a pointer to a calc_state_t object with fields used to
 *          initialize and track the state of a calculation.
 * 
 */
void serv_int_clean_calc_state(calc_state_t *calc_state);


/*****                                       *****
 *****    Protocol functions for messages    *****
 *****                                       *****/ 

/* NOTE: All protocol functions should return (int) 0 unless they error
 *       or are requesting the server to break out of the calculation
 *       loop.
 */

/* 
 *
 */
int protocol_status(connectinfo_t *cnxinfo, calc_state_t *calc_state);

/* 
 *
 */
int protocol_init(connectinfo_t *cnxinfo, calc_state_t *calc_state);

/* 
 *
 */
int protocol_posdata(connectinfo_t *cnxinfo, calc_state_t *calc_state);

/* 
 *
 */
int protocol_getforce(connectinfo_t *cnxinfo, calc_state_t *calc_state);

/* 
 *
 */
int protocol_getstress(connectinfo_t *cnxinfo, calc_state_t *calc_state);

/*
 * Echos the request data back to the client. 
 * 
 * Arguments:
 *  cnxinfo: information about the client-server connection so this
 *          function can send its data.
 *  request: data that will be sent directly back to the client
 */
int protocol_echo(connectinfo_t *cnxinfo);