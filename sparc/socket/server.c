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


int main(int argc, char **argv) {
  int option_char;
  int portno = 20801; /* port to listen on */
  int max_queue = 1;
  server_t *serv;
  
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

  server_serve(&serv);

}

// Will need to expand this more in the future for more codes
const char echo_sig[] = "ECHO ";
const char calc_sig[] = "ASE CALC ";
const char eor_sig[] = "\r\n\r\n";

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

ssize_t serv_int_send(connectinfo_t **cnxinfo, const void *data, size_t len){
    // need to be able to handle large files
    int bytes_sent, bytes_left = len, total=0;
    while(total < len) { //similar to beej's partial send()s example
        // Send client piece of file
        bytes_sent = send((*cnxinfo)->sockfd, data+total, bytes_left, 0);
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

void serv_int_bind_socket(server_t **serv, int *s){
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

int serv_int_listen_accept(server_t **serv, int *s){
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

char *serv_int_parse_request(connectinfo_t **cnxinfo, char req_buf[]){
    char *substr, *request=NULL;
    const char *used_sig;

    /* Check for "ASE CALC " as first characters */
    if (strncmp(calc_sig, req_buf, strlen(calc_sig)) == 0) {  
      (*cnxinfo)->request_type = 1;
      used_sig = calc_sig;
    } 
    /* Check for "ECHO " as first characters */
    else if (strncmp(echo_sig, req_buf, strlen(echo_sig)) == 0){
      (*cnxinfo)->request_type = 2;
      used_sig = echo_sig;
    } 
    else {
      (*cnxinfo)->request_type = -1;
      (*cnxinfo)->STATUS = INVALID;
      fprintf(stderr, "No request string found.\n");
      return request;
    }

    /* Make sure it's a complete request by checking end characters*/
    if ((substr = strstr(req_buf, eor_sig)) == NULL) { 
        (*cnxinfo)->STATUS = INVALID;
        printf("%.128s\n", req_buf);
        fprintf(stderr, "End of request characters not found.\n");
    }
    else {
        int req_len = ((int) (substr - req_buf)) - (strlen(used_sig)) + 1; // might need to subtract 1
        (*cnxinfo)->request_len = req_len;
        request = calloc(1, req_len+1);
        
        strncpy(request, &(req_buf[strlen(used_sig)]), req_len); // might need to shift by 1
    }

    return request;
}

int protocol_echo(connectinfo_t **cnxinfo, char *request){
    int bytes_returned;

    // Echo recieved data back to client
    bytes_returned = serv_int_send(cnxinfo, request, (*cnxinfo)->request_len);
    if (bytes_returned == -1){
      printf("Server failed to return message. \n");
      exit(1);
    }
    return 0;
}

int protocol_calc(connectinfo_t **cnxinfo, char *request) {
     return protocol_echo(cnxinfo, request);
}

int protocol_dispatch(connectinfo_t **cnxinfo, char *request){
    int rv = -1;

    if ((*cnxinfo)->request_type == 1){
      rv = protocol_calc(cnxinfo, request);
    } 
    else if ((*cnxinfo)->request_type == 2){
      rv = protocol_echo(cnxinfo, request); 
    }
    return rv;
}

void server_serve(server_t **serv){
    int s, num; // socket, counter for req_buf bytes_received
    int bytes_recvd = 0;// bytes_sent = 0, ;
    char req_buf[MAX_REQUEST_LEN], tmp_buf[MAX_REQUEST_LEN];

    /* Bind the server to a socket */
    serv_int_bind_socket(serv, &s);

    /* Listen for requests */
    while (1){

        /* Accept a connection from client */
        int sockfd = serv_int_listen_accept(serv, &s);
        if (sockfd == -1){
            printf("Failed to accept");
        }

        /* Recieve the request into tmp_buf, then copying the recieved bytes to the
         * current offset in recv_buf */
        connectinfo_t *cnxinfo_struct = calloc(1, sizeof(connectinfo_t)); 
        connectinfo_t **cnxinfo = &cnxinfo_struct;
        (*cnxinfo)->STATUS = OK; //default ok
        (*cnxinfo)->sockfd = sockfd;
        num = 0;
        memset(&req_buf, '\0', MAX_REQUEST_LEN);
        while (num < MAX_REQUEST_LEN){
            bytes_recvd = recv((*cnxinfo)->sockfd, &tmp_buf, MAX_REQUEST_LEN, 0);
            printf("bytes received: \"%.*s\"\n", bytes_recvd, tmp_buf);
             
            if (bytes_recvd <= 0){ // 0 bytes recieved = closed connection; <0 bytes = error
                printf("%d bytes received. \n", bytes_recvd);
                printf("Client has closed their connection or there was an error. \n\n");
                (*cnxinfo)->STATUS = INVALID; //INVALID
                break;
            } 
            memcpy(&(req_buf[num]), &tmp_buf, bytes_recvd); 
            num += bytes_recvd;
            if (num > MAX_REQUEST_LEN) {
                fprintf(stderr, "Request is too long: %d bytes \n", num);
                (*cnxinfo)->STATUS = INVALID; //INVALID
                exit(1);
            }
            /* Locate a substring with the end-of-response message - if NULL, then the client hasn't
             * finished sending the message */
            if ((memmem(&req_buf, MAX_REQUEST_LEN, &eor_sig, sizeof(eor_sig)-1)) != NULL){ 
                break; //success
            }
        }

        /* Parse the request, decide status */
        // printf("%.128s\n", req_buf);
        char *request = serv_int_parse_request(cnxinfo, req_buf);
        printf("Request Parsed.\n");
        if ((*cnxinfo)->STATUS == INVALID){
            printf("Sending Invalid Status 1\n");
            // serv_int_sendheader(cnxinfo, (*cnxinfo)->STATUS, 0);
            
        } else {
          /* Process request cases*/
          int rv = protocol_dispatch(cnxinfo, request);
          printf("Function return: %d\n", rv);
          if (rv < 0){
              printf("Sending Error Status 2\n");
              (*cnxinfo)->STATUS = ERROR;
              // serv_int_sendheader(cnxinfo, (*cnxinfo)->STATUS, 0);  
          }
        }
        if (request != NULL){
            printf("Freed request.\n");
            free(request);
        }
        if (*cnxinfo != NULL){
            printf("Closed client socket.\n");
            close((*cnxinfo)->sockfd);
            free(cnxinfo_struct);
            *cnxinfo = NULL;
        }
        continue;
    }
    close(s);
    free(*serv);
}

// ssize_t serv_int_sendheader(connectinfo_t **cnxinfo, serverstatus_t status, size_t file_len){
//     printf("File length: %lu\n", file_len);
//     char hdr[MAX_REQUEST_LEN];
//     int hdr_len = 0;
//     char scheme[7] = "SPARC ";
//     //copy scheme to buffer
//     memcpy(hdr, scheme, strlen(scheme));
//     hdr_len += strlen(scheme);
//     switch (status){
//         char *stat;
//         case OK:
//             //copy "OK " to buffer
//             stat = "OK ";
//             memcpy(hdr+hdr_len, stat, strlen(stat));
//             hdr_len += strlen(stat);      
//             //copy file_len - sprintf
//             char fl[35];
//             memset(fl, '\0', 35);
//             sprintf(fl, "%lu",  file_len);
//             memcpy(hdr+hdr_len, fl, strlen(fl));
//             hdr_len += strlen(fl);
//             break;
//         case ERROR:
//             //copy "ERROR" to buffer
//             stat = "ERROR";
//             memcpy(hdr+hdr_len, stat, strlen(stat));
//             hdr_len += strlen(stat); 
//             break;  
//         case INVALID:
//             //copy "INVALID" to buffer
//             stat = "INVALID";
//             memcpy(hdr+hdr_len, stat, strlen(stat));
//             hdr_len += strlen(stat); 
//             break;
//         default:
//             fprintf(stderr, "Incorrect error code.");
//             exit(1);
//     }
//     //copy "\r\n\r\n" to buffer
//     memcpy(hdr+hdr_len, eor_sig, strlen(eor_sig));
//     hdr_len += strlen(eor_sig);
//     int bytes_sent = send((*cnxinfo)->sockfd, hdr, hdr_len, 0);
//     if (bytes_sent == -1){
//         fprintf(stderr, "Server failed to header. \n");
//         return -1;
//     } else {
//         // printf("%d\n", bytes_sent);
//         printf("%.*s", bytes_sent, hdr);
//     } 
//     return bytes_sent;
// }

// void server_set_handlerarg(server_t **serv, void* arg){
//     (*serv)->harg = arg;
// }

// void server_set_handler(server_t **serv, handler_error_t (*handler)(connectinfo_t **, const char *, void*)){
//     (*serv)->handler = handler;
// }

