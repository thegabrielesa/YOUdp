#include "header.h"
#include "../basic.h"
#include "../selective_repeat.h"

int main(int argc, char *argv[]) {
    int listening_socket, res, *is_onthefly;
    pthread_t tid;

    /* check arguments */
    decode_parameters(argc, argv);
    if (DEBUG) {
        printf("***** WINDOW_SIZE=%d | TIMEOUT=%ld && ADAPTIVE=%d | PROB=%d "\
                "*****\n", WINDOW_SIZE, 
                TIMEOUT_RTX.tv_sec * 1000 + TIMEOUT_RTX.tv_usec / 1000, 
                ADAPT, PROB);
    }
    
    /* pkts accepted from any interface and on listening port */
    listening_socket = create_socket(INADDR_ANY, SERV_PORT);

    create_file_list(FILES_DIR);

    /* initialize struct to store client rqst */
    ci = (struct client_info *) allocate_memory(1, sizeof(struct client_info));
    ci->addrlen = sizeof(ci->addr); 

    spawn_threads();
    /* for new thread that will be created */
    *(is_onthefly = allocate_memory(1, sizeof(int))) = 1;
    for (;;) {    
        acquire_mutex(&mtx);
        /* receive and store client rqst */
        receive_packet(listening_socket, &(ci->pkt), sizeof(packet_t), 
                (struct sockaddr *)&(ci->addr), &(ci->addrlen));
        release_mutex(&mtx);

        acquire_mutex(&mtx2);
        if (busy_threads < NUM_THREADS)
            notify(&new_rqst);
        else {
            /* all thread are occupied ==> thread on the fly */
            res = pthread_create(&tid, NULL, thread_job, (void *) is_onthefly);
            abort_on_error(res != 0, "error in pthread_create");
        }
        release_mutex(&mtx2);

        /* allow a worker thread to reiceve notification */
        usleep(1000);
    }

    /* unreachable */
    free(ci);
    close(listening_socket);
    exit(EXIT_SUCCESS);
}

int create_socket(uint32_t hostlong, uint16_t hostshort) {
    int sockfd, res;
    struct sockaddr_in addr;

    /* socket has been created */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    abort_on_error(sockfd < 0, "error in socket");

    /* inizialization of port and address */
    memset((void *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(hostlong); /* network interface */
    addr.sin_port = htons(hostshort);       /* port number */

    /* address has been assigned to socket */
    res = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    abort_on_error(res < 0, "error in bind");

    return sockfd;
}

void spawn_threads(void) {
    int i, res, *is_onthefly;
    pthread_t tid;
            
    /* thread worker will be alive during server's life */
    *(is_onthefly = allocate_memory(1, sizeof(int))) = 0;
    for (i = 0; i < NUM_THREADS; i++) {
        res = pthread_create(&tid, NULL, thread_job, (void *) is_onthefly);
        abort_on_error(res != 0, "error in pthread_create");
    }
}

void *thread_job(void *arg) {
    struct client_info *cur_rqst;
    int res, data_socket, is_onthefly;
    
    is_onthefly = *((int *) arg);
    /* it contains local copy of client rqst */
    cur_rqst = (struct client_info *) allocate_memory(1, 
            sizeof(struct client_info));
    
    /* packets will be accepted from any interface and on a random port */
    data_socket = create_socket(INADDR_ANY, 0);

    for (;;) {
        /* checking if some new request is available */
        acquire_mutex(&mtx);
        while ((ci->pkt).header.type == EMPTY) {
            /* rqst not already available or it has been processed 
               by another thread */
            await(&new_rqst, &mtx);
        }
        memcpy(cur_rqst, ci, sizeof(struct client_info));
        /* it sets rqst as not available anymore */
        (ci->pkt).header.type = EMPTY;
        release_mutex(&mtx);

        if (!is_onthefly) {
            acquire_mutex(&mtx2);
            busy_threads++;
            release_mutex(&mtx2);
        }

        execute_request(data_socket, cur_rqst);

        /* thread terminates */
        if (is_onthefly) {
            /* exit */
            res = pthread_detach(pthread_self());
            abort_on_error(res != 0, "error in pthread_detach");
            break;
        } else {
            acquire_mutex(&mtx2);
            busy_threads--;
            release_mutex(&mtx2);
        }
    }

    free(cur_rqst);
    pthread_exit(NULL);
}

void execute_request(int socket, struct client_info *rqst) {
    int res;
    char filename[MAXLINE + 1], path[MAXLINE + 1];
    off_t file_len;

    srand(time(NULL));
    switch ((rqst->pkt).header.type) {
        case LIST:
            printf("<#> %s:%d requested file list <#>\n", 
                    inet_ntoa((rqst->addr).sin_addr), 
                    ntohs((rqst->addr).sin_port));

            res = send_file_list(socket, 
                    (struct sockaddr *)&(rqst->addr), 
                    rqst->addrlen, (rqst->pkt).header.n_seq);

            if (res > 0) {
                /* transfer OK */
                printf("<!> %s:%d downloaded file list <!>\n", 
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port));
            } else {
                /* max retrasmission reached, transfer aborted */
                printf("<?> %s:%d is unreachable <?>\n", 
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port));
            }
            break;
        case GET:
            printf("<#> %s:%d requested to download '%s' file <#>\n", 
                    inet_ntoa((rqst->addr).sin_addr), 
                    ntohs((rqst->addr).sin_port),
                    (rqst->pkt).payload);

            res = send_file(socket, (rqst->pkt).payload, 
                    (struct sockaddr *) &(rqst->addr), 
                    rqst->addrlen, (rqst->pkt).header.n_seq);

            if (res < 0) {
                /* file doesn't exist on server */
                printf("<?> %s:%d '%s' file not found on server <?>\n",
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port),
                        (rqst->pkt).payload);
            } else if (res == 0) {
                /* max retrasmission reached, transfer aborted */
                printf("<?> %s:%d is unreachable <?>\n", 
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port));
            } else {
                /* trasfer OK */
                printf("<!> %s:%d downloaded '%s' file <!>\n", 
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port),
                        (rqst->pkt).payload);
            }
            break;
        case PUT:
            /* file name and size from request pkt */
            sscanf((rqst->pkt).payload, "%s\t%ld", 
                    filename, &file_len);

            printf("<#> %s:%d requested to upload '%s' file <#>\n", 
                    inet_ntoa((rqst->addr).sin_addr), 
                    ntohs((rqst->addr).sin_port),
                    filename);

            res = receive_file(socket, filename, file_len, 
                    (struct sockaddr *) &(rqst->addr), 
                    rqst->addrlen, (rqst->pkt).header.n_seq);


            if (res < 0) {
                /* file exceeds max uploading size */
                printf("<?> %s:%d upload denied: '%s' file is too big <?>\n",
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port),
                        filename);
            } else if (res == 0) {
                /* connection expired, trasfer aborted */
                printf("<?> %s:%d is unreachable <?>\n", 
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port));
                
                /* build path of uploaded files */
                strcpy(path, FILES_DIR);
                strcat(path, filename);

                /* deleting file */
                abort_on_error(remove(path) < 0, "error in remove");
            } else {
                /* trasfer OK */
                printf("<!> %s:%d uploaded '%s' file <!>\n", 
                        inet_ntoa((rqst->addr).sin_addr), 
                        ntohs((rqst->addr).sin_port),
                        filename);

                update_file_list(filename, file_len);
            }
            break;
    }
}

void create_file_list(const char *path) {
    DIR *dr;
    struct dirent *de;  
    int fd_file, fd_list;
    off_t file_size;
    
    dr = opendir(path);
    /* couldn't open directory */
    abort_on_error(dr == NULL, "error in opendir");

    fd_list = create_file("file_list.bin", SYSTEM_DIR, 0777);
    /* read all elements in directory */
    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            /* current and previous directory are not considered */
            continue;
        }

        fd_file = open_file(de->d_name, path, O_RDONLY);
        file_size = get_file_size(fd_file);
        /* write file name and size on filelist */
        dprintf(fd_list, "%s\t%ld\n", de->d_name, file_size);
        close(fd_file);
    }

    close(fd_list);
    closedir(dr);
}

int send_file_list(int sockfd, const struct sockaddr *dest_addr, 
        socklen_t addrlen, long cur_nseq) {
    packet_t *snd_pkt;
    int fd, acked, outcome;
    off_t file_size;
    
    fd = open_file("file_list.bin", SYSTEM_DIR, O_RDONLY);
    abort_on_error2(fd < 0, "file_list does not exist"); 
    file_size = get_file_size(fd);

    /* build info list pkt */
    snd_pkt = (packet_t *) allocate_memory(1, sizeof(packet_t));
    (snd_pkt->header).type = LIST_ACK;
    (snd_pkt->header).n_seq = cur_nseq + 1;
    sprintf(snd_pkt->payload, "LIST\t%ld", file_size);
    
    /* send info list pkt to client */
    if (DEBUG) printf("LIST_ACK sent, seq: %ld\n", (snd_pkt->header).n_seq);
    //send_packet(sockfd, snd_pkt, sizeof(packet_t), dest_addr, addrlen);
    sendto(sockfd, snd_pkt, sizeof(packet_t), 0, dest_addr, addrlen);

    /* waiting ack from client to start trasmission */
    set_timeout(sockfd, &TIMEOUT_CON);
    acked = wait_ack(sockfd, snd_pkt, LIST, cur_nseq + 1, dest_addr, addrlen);

    outcome = 0;
    if (acked) {
        /* ack received, trasmission starts */
        set_timeout(sockfd, &TIMEOUT_RTX);
        outcome = sender_job(sockfd, NULL, dest_addr, addrlen, fd, file_size);
    }

    free(snd_pkt);
    close(fd);

    return (acked && outcome);
}

int send_file(int sockfd, const char *filename,
        const struct sockaddr *dest_addr, socklen_t addrlen, long cur_nseq) {
    int fd, acked, outcome;
    off_t file_len;
    char result[4];
    packet_t *snd_pkt;

    fd = open_file(filename, FILES_DIR, O_RDONLY);
    if (fd == -1) {
        /* file not found */
        file_len = (off_t) -1;
        strcpy(result, "ERR");
    } else {
        file_len = get_file_size(fd);
        strcpy(result, "OK");
    }

    /* build info file packet */
    snd_pkt = (packet_t *) allocate_memory(1, sizeof(packet_t));
    (snd_pkt->header).type = GET_ACK;
    (snd_pkt->header).n_seq = cur_nseq + 1;
    sprintf(snd_pkt->payload, "GET\t%s\t%ld", result, file_len);

    /* send info file pkt to client */
    if (DEBUG) printf("GET_ACK sent, seq: %ld\n", (snd_pkt->header).n_seq);
    //send_packet(sockfd, snd_pkt, sizeof(packet_t), dest_addr, addrlen);
    sendto(sockfd, snd_pkt, sizeof(packet_t), 0, dest_addr, addrlen);

    outcome = -1;
    if (fd > 0) {
        /* waiting ack from client to start trasmission */
        set_timeout(sockfd, &TIMEOUT_CON);
        outcome = acked = wait_ack(sockfd, snd_pkt, GET, cur_nseq + 1, 
                dest_addr, addrlen);

        if (acked) {
            /* ack received, trasmission starts */
            set_timeout(sockfd, &TIMEOUT_RTX);
            outcome = sender_job(sockfd, NULL, dest_addr, addrlen, 
                    fd, file_len);
        }
    }

    free(snd_pkt);
    close(fd);

    return outcome;
}

int wait_ack(int socket, packet_t *info, char rqst_type, long cur_nseq,
        const struct sockaddr *dest_addr, socklen_t addrlen) {
    int nread, acked;
    packet_t *rcv_pkt;

    rcv_pkt = (packet_t *) allocate_memory(1, sizeof(packet_t));
    for (;;) {
        nread = receive_packet(socket, rcv_pkt, sizeof(packet_t), NULL, NULL);
        if (nread < 0) {
            /* timeout */
            acked = 0;
            break;
        } else if ((rcv_pkt->header).type == rqst_type && 
                (rcv_pkt->header).n_seq == cur_nseq - 1) {
            /* client requests file another time, info file resent */
            if (DEBUG) printf("rtx reply pkt\n");
            send_packet(socket, info, sizeof(packet_t), dest_addr, addrlen);
        } else if ((rcv_pkt->header).type == ACK &&
                (rcv_pkt->header).n_seq == cur_nseq + 1) {
            /* ack received */
            if (DEBUG) printf("ACK received, seq: %ld\n", 
                    (rcv_pkt->header).n_seq);
            acked = 1;
            break;
        }
    }
    free(rcv_pkt);

    return acked;
}


int receive_file(int sockfd, char *filename, off_t file_len,
        const struct sockaddr *dest_addr, socklen_t addrlen, long cur_nseq) {
    int fd, outcome;
    packet_t *snd_pkt;
    char result[4];

    /* check file size */ 
    if (file_len > MAX_FILE_SIZE) strcpy(result, "FTB");
    else strcpy(result, "OK");

    /* build reply pkt with rqst result */
    snd_pkt = (packet_t *) allocate_memory(1, sizeof(packet_t));
    (snd_pkt->header).type = PUT_ACK;
    (snd_pkt->header).n_seq = cur_nseq + 1;
    sprintf(snd_pkt->payload, "PUT\t%s", result);
    
    /* send result pkt to client */
    if (DEBUG) printf("PUT_ACK sent, seq: %ld\n", (snd_pkt->header).n_seq);
    //send_packet(sockfd, snd_pkt, sizeof(packet_t), dest_addr, addrlen);
    sendto(sockfd, snd_pkt, sizeof(packet_t), 0, dest_addr, addrlen);

    outcome = -1;
    if (file_len <= MAX_FILE_SIZE) {
        /* check if filename has been already used */
        acquire_mutex(&mtx3);
        fd = check_filename(filename, FILES_DIR, 0777);
        release_mutex(&mtx3);

        /* waiting to receiving file */
        set_timeout(sockfd, &TIMEOUT_CON);
        outcome = receiver_job(sockfd, NULL, dest_addr, addrlen, fd, file_len);

        close(fd);
    }
    free(snd_pkt);

    return outcome;
}

void update_file_list(const char *filename, off_t file_len) {
    int fd = open_file("file_list.bin", SYSTEM_DIR, O_WRONLY | O_APPEND);
    abort_on_error2(fd < 0, "file list does not exist");      
    dprintf(fd, "%s\t%ld\n", filename, file_len);
    close(fd);
}

void decode_parameters(int argc, char *argv[]) {
    int opt, res;
    long int value;
    const char *usage = "Usage: ./server [-n window] [-p probability(%)] "\
                         "[-t timeout(ms)] [-d] debug";

    /* don't want writing to stderr */
    opterr = 0; 
    while ((opt = getopt(argc, argv, "n:p:t:d")) != -1) {
        switch (opt) {
            case 'n':
                /* WINDOW SIZE */
                res = string_to_long(optarg, &value);
                abort_on_error2(res < 0, "window value not correct");
                abort_on_error2(value <= 0, "window value must be > 0");
                WINDOW_SIZE = (int) value;
                BUFFER_SIZE = WINDOW_SIZE + 1;
                break;
            case 'p':
                /* PROBABILITY (%) */
                res = string_to_long(optarg, &value);
                abort_on_error2(res < 0, "probability value not correct");
                abort_on_error2(value < 0 || value > 100,
                        "probability must be in [0, 100]");
                PROB = (int) value;
                break;
            case 't':
                /* TIMEOUT (ms) */
                res = string_to_long(optarg, &value);
                abort_on_error2(res < 0, "timeout value not correct");
                abort_on_error2(value <= 0, "timeout must be > 0");

                ADAPT = 0;
                TIMEOUT_RTX.tv_sec = value / 1000;
                TIMEOUT_RTX.tv_usec = (value % 1000) * 1000;

                TIMEOUT_CON.tv_sec = (MAX_RTX * value + RTT) / 1000;
                TIMEOUT_CON.tv_usec = ((MAX_RTX * value + RTT) % 1000) * 1000;
                break;
            case 'd':
                DEBUG = 1;
                break;
            default:
                abort_on_error2(1, usage);
        }
    }

    /* we would not have arguments with no options */
    abort_on_error2(optind < argc, usage);
}
