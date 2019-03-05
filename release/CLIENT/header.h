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


#define NUM_THREADS 10
#define NSF 1
#define FTB 2
#define HU 3
#define RUN 4


/* global variables */
/* check if new cmd has been typed */
packet_t *rqst;
pthread_cond_t new_rqst = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
/* number of threads not available */
int busy_threads = 0;
pthread_cond_t done = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx2 = PTHREAD_MUTEX_INITIALIZER;
/* check existing filename */
pthread_mutex_t mtx3 = PTHREAD_MUTEX_INITIALIZER;
/* address and port where to send the cmds */ 
struct sockaddr_in SERVADDR;
/* path where files are stored or from which they are look for (upload) */
const char *FILES_DIR = "./FILES/";
const char *DOWNLOAD_DIR = "./DOWNLOAD/";
const char *SYSTEM_DIR = "./SYSTEM/";


/* prototypes */
void decode_parameters(int argc, char *argv[]);
struct thread_element *spawn_threads(void);
void *client_job(void *arg);
void execute_request(int sockfd, packet_t *rqst, struct thread_element *te);
int check_command(const char *cmd);
int request_packet(const char *command, packet_t *p);
packet_t *wait_reply_packet(int socket, void *rqst_pkt, size_t len,
        long cur_nseq, struct sockaddr *dest_addr, socklen_t dest_len,
        struct sockaddr *src_addr, socklen_t *src_len);
int get_file(int socket, struct thread_element *te, 
        struct sockaddr *dest_addr, socklen_t addrlen, char *filename, 
        off_t file_len, long cur_nseq);
int put_file(int socket, struct thread_element *te, 
        const struct sockaddr *dest_addr, socklen_t addrlen, 
        const char *filename, off_t file_len);
