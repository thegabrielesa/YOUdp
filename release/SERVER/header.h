#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "../types.h"


#define MAX_FILE_SIZE 262144000     /* 250 MB */
#define NUM_THREADS 30


/* global variables */
/* check if new request from client has been received */
struct client_info *ci;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_rqst = PTHREAD_COND_INITIALIZER;
/* number of threads not available */
int busy_threads = 0;
pthread_mutex_t mtx2 = PTHREAD_MUTEX_INITIALIZER;
/* check existing filename */
pthread_mutex_t mtx3 = PTHREAD_MUTEX_INITIALIZER;
/* path where files are stored or from which they are look for */
const char *FILES_DIR = "./FILES/";
const char *SYSTEM_DIR = "./SYSTEM/";


/* prototypes */
void decode_parameters(int argc, char *argv[]);
int create_socket(uint32_t hostlong, uint16_t hostshort);
void create_file_list(const char *path);
int send_file(int sockfd, const char *filename,
        const struct sockaddr *dest_addr, socklen_t addrlen, long cur_nseq);
int receive_file(int sockfd, char *filename, off_t file_len,
        const struct sockaddr *dest_addr, socklen_t addrlen, long cur_nseq);
int send_file_list(int sockfd, const struct sockaddr *dest_addr, 
        socklen_t addrlen, long cur_nseq);
int wait_ack(int socket, packet_t *rqst_pkt, char rqst_type, long cur_nseq,
        const struct sockaddr *dest_addr, socklen_t addrlen);
void update_file_list(const char *filename, off_t file_len);
void spawn_threads(void);
void *thread_job(void *arg);
void execute_request(int socket, struct client_info *rqst);
