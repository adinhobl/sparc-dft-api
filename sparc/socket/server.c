#define _GNU_SOURCE
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>

#include "server.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  server [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 20801)\n"                               \
"  -m                  Maximum pending connections (default: 1)\n"            \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"max_queue",     required_argument,      NULL,           'm'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

const char eor_sig[] = "\r\n\r\n";

int main(int argc, char **argv) {
  int option_char;
  int portno = 20801;    // port to listen on
  int max_queue = 1;     // max number of connections to server
  int s;                // socket fd that will eventually be filled
  server_t *serv;
  connectinfo_t *cnx;
    
  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
   switch (option_char) {
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      default:
        fprintf(stderr, "%s ", USAGE);
        exit(1);
      case 'm': // server
        max_queue = atoi(optarg);
        break; 
      case 'h': // help
        fprintf(stdout, "%s ", USAGE);
        exit(0);
        break;
    }
  }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    if (max_queue < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, max_queue);
        exit(1);
    }


  serv = server_create();

  server_set_port(&serv, portno);
  server_set_maxqueue(&serv, max_queue);

  cnx = server_connect(&serv, &s);
  server_process(cnx); //processes in potentially inf loop

  return server_abort(cnx, serv, s);
}

server_t* server_create(){
    server_t *serv = calloc(1, sizeof(server_t));
    memset(&(serv->hints), 0, sizeof(struct addrinfo));
    serv->hints.ai_family = AF_UNSPEC;
    serv->hints.ai_socktype = SOCK_STREAM;
    serv->hints.ai_flags = AI_PASSIVE;
    return serv;
}

void server_set_port(server_t **serv, unsigned short port){
    sprintf((*serv)->port, "%d", port);
}

void server_set_maxqueue(server_t **serv, int max_queue){
    (*serv)->max_queue = max_queue;
}

connectinfo_t *server_connect(server_t **serv, int *s){

    /* Bind the server to a socket */
    serv_bind_socket(serv, s);

    /* Accept a connection from client */
    int sockfd = serv_listen_accept(serv, s);
    if (sockfd == -1){
        printf("Failed to accept");
    }

    /* Create a connection info struct for the session */    
    connectinfo_t *cnxinfo = calloc(1, sizeof(connectinfo_t)); 
    cnxinfo->cnx_status = OK; //default ok
    cnxinfo->sockfd = sockfd;

    return cnxinfo;
}

void server_process(connectinfo_t *cnxinfo){
    char req_buf[TMP_BUF_SIZE];
    int rv = 0;
    calc_state_t calc_state;
    
    serv_init_calc_state(&calc_state);

    /* For each interaction - continues until abort call or error*/
    while (rv == 0){

        memset(&req_buf, '\0', TMP_BUF_SIZE);
        serv_recv_msg(cnxinfo, req_buf, TMP_BUF_SIZE);

        /* Parse the request, decide status */
                // printf("%.128s\n", req_buf);
        serv_parse_request(cnxinfo, req_buf);
        // printf("Request Parsed.\n");
        if (cnxinfo->cnx_status == INVALID){
            printf("Invalid Request, attempt a different call.\n");
            continue;
        }

        /* Process request cases*/
        rv = serv_dispatch(cnxinfo, &calc_state);
        // printf("Function return: %d\n", rv);

    }

    serv_clean_calc_state(&calc_state);
}

int server_abort(connectinfo_t *cnx, server_t *serv, int s){
    close(s);
    free(cnx);
    free(serv);

    return 0;
}

ssize_t serv_send(connectinfo_t *cnxinfo, const void *data, size_t len){
    // need to be able to handle large files
    int bytes_sent, bytes_left = len, total=0;
    while(total < len) { //similar to beej's partial send()s example
        // Send client piece of file
        bytes_sent = send(cnxinfo->sockfd, data+total, bytes_left, 0);
        // printf("%.*s", bytes_sent, buf);
        if (bytes_sent == -1){
            printf("Bytes sent in last message: %d ", bytes_sent);
            printf("Total bytes sent: %d ", total);
            fprintf(stderr, "Server failed to send message. \n");
            // return total;
            bytes_sent = 0;
        } 
        total += bytes_sent;
        bytes_left -= bytes_sent;
    }
    printf("Bytes sent: %d \n", total);
    return total;
}

ssize_t serv_recv_msg(connectinfo_t *cnxinfo, char *req_buf, int max_size){ 
    int bytes_recvd = 0; // # of bytes_received this msg
    int total_num = 0; // counter for req_buf 
    char tmp_buf[TMP_BUF_SIZE];

    /* Recieve the request into tmp_buf, then copying the recieved bytes to the
    * current offset in recv_buf */
    while (total_num < max_size){
        bytes_recvd = recv(cnxinfo->sockfd, &tmp_buf[0], TMP_BUF_SIZE, 0);
        printf("bytes received: \"%.*s\"\n", bytes_recvd, tmp_buf);
            
        if (bytes_recvd <= 0){ // 0 bytes recieved = closed connection; <0 bytes = error
            printf("%d bytes received. \n", bytes_recvd);
            printf("Client has closed their connection or there was an error. \n\n");
            cnxinfo->cnx_status = INVALID; //INVALID
            break;
        } 
        memcpy(&(req_buf[total_num]), &tmp_buf, bytes_recvd); 
        total_num += bytes_recvd;
        if (total_num > max_size) {
            fprintf(stderr, "Request is too long: %d bytes \n", total_num);
            cnxinfo->cnx_status = INVALID; //INVALID
            exit(1);
        }
        /* Locate a substring with the end-of-response message - if NULL, then the client hasn't
         * finished sending the message */
        if ((memmem(req_buf, total_num, &eor_sig, sizeof(eor_sig)-1)) != NULL){ 
            break; //success
        }
    }
    return total_num;
}

ssize_t serv_recv_arr(connectinfo_t *cnxinfo, double *arr_buf, int num_elements){
    char tmp_buf[9 * sizeof(double)];
    int bytes_recvd = 0; // # of bytes_received this msg
    int total_bytes = 0; // counter for tmp_buf 
    int desired_bytes = sizeof(double) * num_elements;
    void *tmp_ptr = arr_buf;

    /* Recieve the request into tmp_buf, then copying the recieved bytes to the
    * current offset in recv_buf */
    while (total_bytes < desired_bytes){
        bytes_recvd = recv(cnxinfo->sockfd, &tmp_buf[0], sizeof(tmp_buf), 0);
            
        if (bytes_recvd <= 0){ // 0 bytes recieved = closed connection; <0 bytes = error
            printf("%d bytes received. \n", bytes_recvd);
            printf("Client has closed their connection or there was an error. \n\n");
            cnxinfo->cnx_status = INVALID; //INVALID
            break;
        } 
        memcpy((tmp_ptr + total_bytes), &tmp_buf, bytes_recvd); 
        total_bytes += bytes_recvd;
        if (total_bytes > desired_bytes) {
            fprintf(stderr, "Message is too long: %d bytes \n", total_bytes);
            cnxinfo->cnx_status = INVALID; //INVALID
            exit(1);
        }
    }
    return total_bytes;
}

ssize_t serv_recv_int(connectinfo_t *cnxinfo, int *int_ptr){
    char tmp_buf[sizeof(int32_t)];
    int bytes_recvd = 0; // # of bytes_received this msg
    int total_bytes = 0; // counter for tmp_buf 
    int desired_bytes = sizeof(int32_t);
    void *tmp_ptr = int_ptr;

    /* Recieve the request into tmp_buf, then copying the recieved bytes to the
    * current offset in recv_buf */
    while (total_bytes < desired_bytes){
        bytes_recvd = recv(cnxinfo->sockfd, &tmp_buf[0], sizeof(tmp_buf), 0);
            
        if (bytes_recvd <= 0){ // 0 bytes recieved = closed connection; <0 bytes = error
            printf("%d bytes received. \n", bytes_recvd);
            printf("Client has closed their connection or there was an error. \n\n");
            cnxinfo->cnx_status = INVALID; //INVALID
            break;
        } 
        memcpy((tmp_ptr + total_bytes), &tmp_buf, bytes_recvd); 
        total_bytes += bytes_recvd;
        if (total_bytes > desired_bytes) {
            fprintf(stderr, "Message is too long: %d bytes \n", total_bytes);
            cnxinfo->cnx_status = INVALID; //INVALID
            exit(1);
        }
    }
    return total_bytes;
}

void serv_bind_socket(server_t **serv, int *s){
    // Define the Address
    int gai_status, b; // for bind
    int buff = 1; // for setsockopt
    struct addrinfo *res;
    gai_status = getaddrinfo(NULL, (*serv)->port, &(*serv)->hints, &res);
    if (gai_status != 0){ // you have an error
        fprintf(stderr, "getaddrinfo error %d: %s", gai_status, gai_strerror(gai_status));
        fprintf(stderr, "\n");
        exit(1);
    }
    // Bind to a socket
    *s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*s == -1){
        fprintf(stderr, "Couldn't create socket: %s\n", strerror(errno));
    } else {
        // printf("Socket file descriptor: %d\n", s);
    }
    if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &buff, sizeof(buff)) != 0){
        fprintf(stderr, "Address couldn't be reused for socket: %s\n", strerror(errno));
    }
    b = bind(*s, res->ai_addr, res->ai_addrlen);
    if (b != 0){
        fprintf(stderr, "Socket failed to bind: %s\n", strerror(errno));
        // exit(1);
    }
    freeaddrinfo(res);
}

int serv_listen_accept(server_t **serv, int *s){
    int sockfd;
    int l; // for listen
    struct sockaddr_storage claddr; //client address
    socklen_t claddr_size = sizeof(claddr);
    // Listen for a response
    l = listen(*s, (*serv)->max_queue);
    if (l != 0){
        fprintf(stderr, "Socket failed to listen: %s\n", strerror(errno));
    }
    // Accept connection from client
    sockfd = accept(*s, (struct sockaddr *)&claddr, &claddr_size); // cast from beej
    if (sockfd == -1){
        fprintf(stderr, "Socket failed to accept client: %s\n", strerror(errno));
        return -1;
    }
    printf("\nNew client connection.\n");
    return sockfd;
}

void serv_parse_request(connectinfo_t *cnxinfo, char req_buf[]){
    char *substr;

    /* Make sure it's a complete request by checking end characters */
    if ((substr = strstr(req_buf, eor_sig)) == NULL) { 
        cnxinfo->cnx_status = INVALID;
        printf("%.128s\n", req_buf);
        fprintf(stderr, "End of request characters not found.\n");
        exit(1);
    }

    /* Serves as a router for request handling - check if these are first characters*/
    if (strncmp("STATUS", req_buf, strlen("STATUS")) == 0) {  // might need to subtract 1 from len
      cnxinfo->request_type = STATUS;
    } else if (strncmp("INIT", req_buf, strlen("INIT")) == 0){
      cnxinfo->request_type = INIT;
    } else if (strncmp("POSDATA", req_buf, strlen("POSDATA")) == 0){
      cnxinfo->request_type = POSDATA;
    } else if (strncmp("GETFORCE", req_buf, strlen("GETFORCE")) == 0){
      cnxinfo->request_type = GETFORCE;
    } else if (strncmp("GETSTRESS", req_buf, strlen("GETSTRESS")) == 0){
      cnxinfo->request_type = GETSTRESS;
    } else if (strncmp("ABORT", req_buf, strlen("ABORT")) == 0){
      cnxinfo->request_type = ABORT;
    } else if (strncmp("ECHO", req_buf, strlen("ECHO")) == 0){
      cnxinfo->request_type = ECHO;
    } else {
      cnxinfo->request_type = -1;
      cnxinfo->cnx_status = INVALID;
      fprintf(stderr, "No request string found.\n");
    }
}

int serv_dispatch(connectinfo_t *cnxinfo, calc_state_t *calc_state){
    int rv = -1;

    switch(cnxinfo->request_type) {

        case STATUS:
            rv = protocol_status(cnxinfo, calc_state);
            break;
            
        case INIT:
            rv = protocol_init(cnxinfo, calc_state);
            break;
        
        case POSDATA:
            rv = protocol_posdata(cnxinfo, calc_state);
            break;
        
        case GETFORCE: 
            rv = protocol_echo(cnxinfo);
            break;

        case GETSTRESS: 
            rv = protocol_echo(cnxinfo);
            break;
        
        case ABORT:
            rv = 1; // simply want to shut server down without interactions
            break;

        case ECHO:
            rv = protocol_echo(cnxinfo);
            break;

    }
    return rv;
}

void serv_init_calc_state(calc_state_t *calc_state){
    calc_state->bead_index = -1;
    calc_state->init_string_len = 0;
    calc_state->init_string = NULL;
    // calc_state->cell_matrix; // uninitialized
    // calc_state->inv_matrix; // uninitialized
    calc_state->num_atoms = 0;
    calc_state->atom_coords = NULL; 
    calc_state->potential = 0.0;
    // calc_state->virial; // uninitialized
    // JSON string_len
    // JSON String
}

void serv_clean_calc_state(calc_state_t *calc_state){
    if (calc_state->init_string != NULL){
        free(calc_state->init_string);
    }

    if (calc_state->atom_coords != NULL){
        free(calc_state->atom_coords);
    }
}

int protocol_status(connectinfo_t *cnxinfo, calc_state_t *calc_state){
    int rv, bytes_sent;
    if (calc_state->bead_index == -1){
        bytes_sent = serv_send(cnxinfo, "NEEDINIT", strlen("NEEDINIT")+1);
    } else if (calc_state->num_atoms == 0){
        bytes_sent = serv_send(cnxinfo, "READY", strlen("READY")+1);
    } else {
        bytes_sent = serv_send(cnxinfo, "HAVEDATA", strlen("HAVEDATA")+1);
    }

    if (bytes_sent > 0){
        rv = 0;
    } else {
        rv = 1;
    }
    return rv;
}

int protocol_init(connectinfo_t *cnxinfo, calc_state_t *calc_state){
    char req_buf[TMP_BUF_SIZE];
    int bytes_rcvd = 0;
    char *new_init_str_ptr;
    
    // bead index
    bytes_rcvd = serv_recv_msg(cnxinfo, req_buf, TMP_BUF_SIZE);
    printf("bead bytes rcvd: %d\n", bytes_rcvd);
    calc_state->bead_index = atoi(req_buf);
    printf("Bead num: %d\n", calc_state->bead_index);

    // initialization string length
    bytes_rcvd = serv_recv_msg(cnxinfo, req_buf, TMP_BUF_SIZE);
    printf("Bytes rcvd: %d\n", bytes_rcvd);
    calc_state->init_string_len = atoi(req_buf); // only message len, not eor_sig
    printf("Init_str_len: %d\n", calc_state->init_string_len);

    // initialization string
    if (calc_state->init_string_len > 0){
        int new_str_bytes = sizeof(char) * (calc_state->init_string_len + strlen(eor_sig));
        printf("new_str_len: %d\n", new_str_bytes); // not null terminated at this point.
        new_init_str_ptr = realloc(calc_state->init_string, new_str_bytes);
        if (new_init_str_ptr == NULL){
            cnxinfo->request_type = -1;
            cnxinfo->cnx_status = INVALID;
            fprintf(stderr, "Allocation of initialization string failed.\n");
            return -1;
        }

        calc_state->init_string = new_init_str_ptr;
        bytes_rcvd = serv_recv_msg(cnxinfo, calc_state->init_string, new_str_bytes);

        if (bytes_rcvd != new_str_bytes){
            cnxinfo->request_type = -1;
            cnxinfo->cnx_status = INVALID;
            fprintf(stderr, "Full init string not receved.\n");
            return -1;
        }
    } else if (calc_state->init_string != NULL && calc_state->init_string_len == 0){
        // the newest request has no init string, but previous one did
        // may not need this part?
        free(calc_state->init_string);
        calc_state->init_string = NULL;
    } else if (calc_state->init_string_len < 0){
        // not an actual positive string length
        cnxinfo->request_type = -1;
        cnxinfo->cnx_status = INVALID;
        fprintf(stderr, "String length is negative.\n");
        return -1;
    }
    //no init string, none previously

    return 0;
}

int protocol_posdata(connectinfo_t *cnxinfo, calc_state_t *calc_state){
    int num_atoms;

    // 9 doubles for cell vector matrix
    serv_recv_arr(cnxinfo, calc_state->cell_matrix, 9);

    for (int ii=0; ii<9; ii++){
        printf("%f\n", calc_state->cell_matrix[ii]);
    }

    // 9 doubles for inverse matrix
    serv_recv_arr(cnxinfo, calc_state->inv_matrix, 9);

    for (int ii=0; ii<9; ii++){
        printf("%f\n", calc_state->inv_matrix[ii]);
    }

    // 1 int32 for number of atoms
    serv_recv_int(cnxinfo, &num_atoms);
    printf("num atoms: %d\n", num_atoms);

    // 3 doubles for cartesian coords of each atom
    if (num_atoms > 0){
        int atoms_bytes = sizeof(double) * num_atoms * 3;
        printf("atoms_bytes: %d\n", atoms_bytes); 
        double *new_atoms_ptr = realloc(calc_state->atom_coords, atoms_bytes);
        if (new_atoms_ptr == NULL){
            cnxinfo->request_type = -1;
            cnxinfo->cnx_status = INVALID;
            fprintf(stderr, "Allocation of initialization string failed.\n");
            return -1;
        }

        calc_state->atom_coords = new_atoms_ptr;
        int bytes_rcvd = serv_recv_arr(cnxinfo, calc_state->atom_coords, 3 * num_atoms);

        if (bytes_rcvd != atoms_bytes){
            cnxinfo->request_type = -1;
            cnxinfo->cnx_status = INVALID;
            fprintf(stderr, "Full init string not receved.\n");
            return -1;
        }


        for (int ii=0; ii<3 * num_atoms; ii++){
            printf("%f\n", calc_state->atom_coords[ii]);
        }
    }

    return 0;
}

int protocol_echo(connectinfo_t *cnxinfo){
    int bytes_rcvd, bytes_returned;
    char msg_from_client[TMP_BUF_SIZE];

    // Recieve Client calls
    bytes_rcvd = serv_recv_msg(cnxinfo, msg_from_client, TMP_BUF_SIZE);
    printf("Bytes rcvd: %d\n", bytes_rcvd);

    // Echo recieved data back to client
    bytes_returned = serv_send(cnxinfo, msg_from_client, bytes_rcvd);
    if (bytes_returned == -1){
      printf("Server failed to return message. \n");
      exit(1);
    }
    return 0;
}

/*****                                               *****
 *****  Insert Calls to DFT Library, as necessary    *****
 *****                                               *****/ 