#define SERV_PORT 5193
#define DIM_PAYLOAD MAX_PACKET_SIZE - sizeof(header_t)
#define EMPTY 0
#define LIST 1
#define GET 2
#define PUT 3
#define DATA 4
#define ACK 5
#define FIN 6
#define LIST_ACK 7
#define GET_ACK 8
#define PUT_ACK 9
#define FINACK 10
#define MAX_RTX 15
#define RTT 1               /* ms */
#define T_MIN 4             /* ms */
#define T_MAX 1000          /* ms */


/* global variables */
/* default settings */
int WINDOW_SIZE = 16;
int BUFFER_SIZE = 17;
struct timeval TIMEOUT_RTX = {T_MIN / 1000, (T_MIN % 1000) * 1000}; 
struct timeval TIMEOUT_CON = {(MAX_RTX * T_MAX + RTT) / 1000, 
    ((MAX_RTX * T_MAX + RTT) % 1000) * 1000};
int ADAPT = 1;                                  /* adaptive timeout */ 
int DEBUG = 0;
int PROB = 0;                                   /* % pkt loss probability */
/*
+-----------------------------------------------------------------------------+
|   struct timeval {                                                          |
|       time_t {aka long int}           tv_sec      (seconds)                 |
|       suseconds_t {aka long int}      tv_usec     (microseconds)            |
|   };                                                                        |
+-----------------------------------------------------------------------------+
*/


/* prototypes */
/* auxiliary methods */
void abort_on_error(int cond, const char *msg);
void abort_on_error2(int cond, const char *msg);
void *allocate_memory(size_t nmemb, size_t size);
int string_to_long(const char *str, long int *v);
/* socket methods */
ssize_t send_packet(int sockfd, const void *buf, size_t len,
        const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t receive_packet(int socket, void *buf, size_t len, 
        struct sockaddr *src_addr, socklen_t *addrlen);
void set_timeout(int sockfd, struct timeval *timeout);
void get_timeout(int sockfd, struct timeval *timeout);
/* I/O methods */
int open_file(const char *filename, const char *folder_path, int flags);
int create_file(const char *filename, const char *folder_path, mode_t mode);
off_t get_file_size(int fd);
int check_filename(char *filename, const char *folder_path, mode_t mode);
ssize_t full_write(int fd, const char *buf, size_t count);
ssize_t full_read(int fd, char *buf, size_t count);
/* threads methods */
void await(pthread_cond_t *cond, pthread_mutex_t *mutex);
void notify(pthread_cond_t *cond);
void acquire_mutex(pthread_mutex_t *mtx);
void release_mutex(pthread_mutex_t *mtx);


/******************************************************************************
**                                                                           **
**                         Auxiliary methods                                 **
**                                                                           **
******************************************************************************/
void abort_on_error(int cond, const char *msg) {
    if (cond) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void abort_on_error2(int cond, const char *msg) {
    if (cond) {
        fprintf(stderr, "%s\n", msg);
        exit(EXIT_FAILURE);
    }
}

void *allocate_memory(size_t nmemb, size_t size) {
    void *p = calloc(nmemb, size);
    abort_on_error(p == NULL, "error in calloc");
    return p;
}

int string_to_long(const char *str, long int *v) {
    long value;
    char *errptr;

    errno = 0;
    /* convert in 10 base */
    value = strtol(str, &errptr, 10);
    /* invalid character */
    if (errno != 0 || *errptr != '\0') return -1;
    /* successful conversion */
    if (v != NULL) *v = value;
    
    return 0;
}


/******************************************************************************
**                                                                           **
**                            Socket methods                                 **
**                                                                           **
******************************************************************************/
ssize_t send_packet(int sockfd, const void *buf, size_t len,
        const struct sockaddr *dest_addr, socklen_t addrlen) {
    ssize_t nwritten;
    int rnd;

    /* random number between 1 and 100 */
    rnd = rand() % 100 + 1; 
    if (rnd <= PROB) {
        /* simulate packet loss */
        nwritten = 0;
        if (DEBUG) {
            printf("+----------------------+\n");
            printf("|     packet lost!     |\n");
            printf("+----------------------+\n");
        }
    } else {
        /* send packet */
        nwritten = sendto(sockfd, buf, len, 0, dest_addr, addrlen);
        abort_on_error(nwritten < 0, "error in sendto");
        abort_on_error2(nwritten != (ssize_t) len, 
                "sendto: packet writing not full!");
    }

    return nwritten;
}

ssize_t receive_packet(int socket, void *buf, size_t len, 
        struct sockaddr *src_addr, socklen_t *addrlen) {
    ssize_t nread;

    nread = recvfrom(socket, buf, len, 0, src_addr, addrlen);
    /* abort if the error is not timeout expire */
    abort_on_error(nread < 0 && errno != EAGAIN, "error in recvfrom");
    abort_on_error2(nread >= 0 && 
            nread != sizeof(packet_t) && nread != sizeof(ack_t), 
            "recvfrom: packet reading not full!");
    return nread;
}

void set_timeout(int sockfd, struct timeval *timeout) {
    int res = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *) timeout, 
                sizeof(struct timeval));
    abort_on_error(res < 0, "error in setsockopt");
}

void get_timeout(int sockfd, struct timeval *timeout) {
    socklen_t len = sizeof(struct timeval);
    int res = getsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *) timeout, 
            &len);
    abort_on_error(res < 0, "error in getsockopt");
}


/******************************************************************************
**                                                                           **
**                               I/O methods                                 **
**                                                                           **
******************************************************************************/
int open_file(const char *filename, const char *folder_path, int flags) {
    char path[MAXLINE + 1] = {0};
    int fd;

    /* build pathname */
    strcat(path, folder_path);
    strcat(path, filename);
    fd = open(path, flags);
    /* aborting if error is different from file doesn't exist */
    abort_on_error(fd == -1 && errno != ENOENT, "error in open");

    return fd;
}

int create_file(const char *filename, const char *folder_path, mode_t mode) {
    char path[MAXLINE + 1] = {0};
    int fd;

    /* build pathname */
    strcat(path, folder_path);
    strcat(path, filename);
    /* create file and truncate to zero if it already exists */
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    abort_on_error(fd == -1, "error in open");

    return fd;
}

off_t get_file_size(int fd) {
    off_t fs, res;

    fs = lseek(fd, 0, SEEK_END);
    abort_on_error(fs == (off_t) -1, "error in lseek");
    /* placing cursor at file starting point */
    res = lseek(fd, 0, SEEK_SET);
    abort_on_error(res == (off_t) -1, "error in 2nd lseek");

    return fs;
}

int check_filename(char *filename, const char *folder_path, mode_t mode) {
    char name[MAXLINE + 1];
    int i, res, fd;

    strcpy(name, filename);
    /* first entry for new file */
    i = 1;
    for (;;) {
        res = open_file(filename, folder_path, O_RDONLY);
        if (res == -1) {
            /* file does not exist */
            fd = create_file(filename, folder_path, mode);
            break;
        } else {
            /* modify filename */
            sprintf(filename, "%d_%s", i, name);
            i++;
        }
    }

    return fd;
}

ssize_t full_write(int fd, const char *buf, size_t count) {
    size_t nleft;
    ssize_t nwritten;

    nleft = count;
    /* repeat until no left */
    while (nleft > 0) {             
        if ((nwritten = write(fd, buf, nleft)) < 0) {
            /* if interrupted by system call */
	    if (errno == EINTR) {  	
                /* repeat the loop */
                continue;           
            } else {
                /* exit with error */
	        return nwritten;
            }	
        }
        /* set left to write */
	nleft -= nwritten;   
        /* set pointer */
        buf +=nwritten;                
    }

    return nleft;
}

ssize_t full_read(int fd, char *buf, size_t count) {
    size_t nleft;
    ssize_t nread;

    nleft = count;
    /* repeat until no left */
    while (nleft > 0) {            
        if ((nread = read(fd, buf, nleft)) < 0) {
            /* if interrupted by system call */
	    if (errno == EINTR) {
                /* repeat the loop */
                continue;           
            } else {
                /* exit with error */
                return nread;
            } 
        /* EOF, break loop here */
        } else if (nread == 0) break;

        /* set left to read */
	nleft -= nread;         
        /* set pointer */
        buf +=nread;                
    }

    return nleft;
}


/******************************************************************************
**                                                                           **
**                            Thread methods                                 **
**                                                                           **
******************************************************************************/
void await(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    int res = pthread_cond_wait(cond, mutex);
    abort_on_error(res != 0, "error in pthread_cond_wait");
}

void notify(pthread_cond_t *cond) {
    int res = pthread_cond_signal(cond);
    abort_on_error(res != 0, "error in pthread_cond_signal");
}

void acquire_mutex(pthread_mutex_t *mtx) {
    int res = pthread_mutex_lock(mtx);
    abort_on_error(res != 0, "error in pthread_mutex_lock");
}

void release_mutex(pthread_mutex_t *mtx) {
    int res = pthread_mutex_unlock(mtx);
    abort_on_error(res != 0, "error in pthread_mutex_unlock");
}
