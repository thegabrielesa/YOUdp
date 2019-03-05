#define MAX_PACKET_SIZE 1500        /* sizeof(header_t) < MPS < 65507 */
#define MAXLINE 1024

typedef struct {
    char type;
    long n_seq;
    int length;                     /* real payload lenght in bytes */
} header_t;

typedef struct {
    header_t header;
    char payload[MAX_PACKET_SIZE - sizeof(header_t)];
} packet_t;

typedef struct {
    header_t header;
} ack_t;

struct circular_buffer {
    int S;                          /* position of first pkt to send */
    int E;                          /* fist free position to store new pkts */
    int N;                          /* position older pkt not acked */
    packet_t *window;
    int *acked;         	    /* pkts status (acked or not) */
};

struct client_info {
    pthread_t tid;
    struct sockaddr_in addr;
    socklen_t addrlen;
    packet_t pkt;                   /* request pkt from client */
};

struct thread_element {
    pthread_t tid;
    char status;
    char filename[MAXLINE + 1];
    long cur_pkt;                   /* currently pkts sent/received */
    long tot_pkt;                   /* total pkt to send/receive */
    struct timeval start;           /* started time of trasmission */
    struct timeval now;             /* currently time of trasmission */
};
