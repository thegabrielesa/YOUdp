#include "header.h"
#include "../basic.h"
#include "../selective_repeat.h"
#include "helper.h"

int main(int argc, char *argv[]) {
    int res;
    char input[MAXLINE + 1];
    struct thread_element *te;
    
    /* check arguments */
    decode_parameters(argc, argv);
    printf("\t__________________________________________________\n"\
           "\t__________________________________________________\n"\
           "\t________YY___YY__OOOO__U____U_____d_______________\n"\
           "\t_________YY_YY__O____O_U____U_____d_______________\n"\
           "\t___________Y____O____O_U____U__dddd_pppp__________\n"\
           "\t___________Y____O____O_U____U_d___d_p___p_________\n"\
           "\t___________Y_____OOOO___UUUU___dddd_pppp__________\n"\
           "\t____________________________________p_____________\n"\
           "\t____________________________________p_____________\n"\
           "\t__________________________________________________\n\n");    
    printf("YOUdp 1.0\nType \"help\" or \"credits\" for more information\n\n");
    if (DEBUG) {
        printf("***** WINDOW_SIZE=%d | TIMEOUT=%ld && ADAPTIVE=%d | PROB=%d "\
                "*****\n", WINDOW_SIZE, 
                TIMEOUT_RTX.tv_sec * 1000 + TIMEOUT_RTX.tv_usec / 1000, 
                ADAPT, PROB);
    }

    /* pkt that contains client's request */
    rqst = (packet_t *) allocate_memory(1, sizeof(packet_t));
    te = spawn_threads();    
    for (;;) {
        printf(">>> ");
        if (fgets(input, MAXLINE, stdin) == NULL) {
            /* standard input closed (Ctrl-D) */
            printf("\n\n");
            break;
        }
        if (strcmp(input, "exit\n") == 0) {
            /* user quits program */
            printf("\n");
            break;
        }
        if (strcmp(input, "stat\n") == 0) {
            /* show status download/upload */
            print_progress(te);
            continue;
        }
        if (strcmp(input, "help\n") == 0) {
            /* show application usage */
            print_help();
            continue;
        }
        if (strcmp(input, "credits\n") == 0) {
            /* show credits */
            print_credits();
            continue;
        }
        if (!check_command(input)) {
            /* unrecognize command */
            if (strcmp(input, "\n") != 0)
                fprintf(stderr, "Usage: get [file] | put [file] | list | "\
                        "stat\n");
            continue;
        }

        /* parse command and store in pkt */
        acquire_mutex(&mtx);
        res = request_packet(input, rqst);
        release_mutex(&mtx);
        if (res == -1) {
            /* empty filename */
            printf("Error: no filename typed\n");
            continue;
        } else if (res == 1) {
            /* put error, file not found */
            printf("Error: file not found\n");
            continue;
        }

        /* correct command */
        acquire_mutex(&mtx2);
        if (busy_threads < NUM_THREADS) {
            printf("Request scheduled\n");
            notify(&new_rqst);
        } else {
            /* no threads available */
            printf("Maximum simultaneous threads reached\n");
        }
        release_mutex(&mtx2);
    }

    /* check if all threads are free */
    acquire_mutex(&mtx2);
    while (busy_threads > 0) {
        /* there are busy threads */
        printf("\rWaiting %2d thread(s) to terminate...", busy_threads);
        fflush(stdout);
        /* waiting until a thread signal its finishing status */
        await(&done, &mtx2);
    }
    release_mutex(&mtx2);
    printf("\nBye!\n");

    free(rqst);
    free(te);
    exit(EXIT_SUCCESS);
}

struct thread_element *spawn_threads(void) {
    int res;
    struct thread_element *te, *p;

    te = (struct thread_element *) allocate_memory(NUM_THREADS, 
            sizeof(struct thread_element));
    for (p = te; p < te + NUM_THREADS; p++) {
        res = pthread_create(&(p->tid), NULL, client_job, (void *) p);
        abort_on_error(res != 0, "error in pthread_create");
    }

    return te;
}

void *client_job(void *arg) {
    struct thread_element *te;
    packet_t *cur_rqst;
    int sockfd;

    /* it contains information about trasmission progress */
    te = (struct thread_element *) arg;
    /* local copy of request */
    cur_rqst = (packet_t *) allocate_memory(1, sizeof(packet_t));
            
    /* create socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    abort_on_error(sockfd < 0, "error in socket");

    for (;;) {
        /* checking if some request is available */
        acquire_mutex(&mtx);
        while ((rqst->header).type == EMPTY) {
            /* rqst not already available or it has been processed 
               by another thread */
            await(&new_rqst, &mtx);
        }
        memcpy(cur_rqst, rqst, sizeof(packet_t));
        /* set request packet as not available anymore */
        (rqst->header).type = EMPTY;
        release_mutex(&mtx);

        acquire_mutex(&mtx2);
        busy_threads++;
        release_mutex(&mtx2);

        execute_request(sockfd, cur_rqst, te);

        /* thread has done his work */
        acquire_mutex(&mtx2);
        busy_threads--;
        notify(&done);
        release_mutex(&mtx2);
    }

    free(cur_rqst);
    close(sockfd);
    pthread_exit(NULL);
}

void execute_request(int sockfd, packet_t *rqst, struct thread_element *te) {
    struct sockaddr_in new_servaddr;
    socklen_t len;
    char filename[MAXLINE + 1], path[MAXLINE + 1], result[4];
    off_t file_len;
    packet_t *rpl;
    int res;

    srand(time(NULL));
    set_timeout(sockfd, &TIMEOUT_RTX);
   
    switch ((rqst->header).type) {
        case LIST:
            /* file name to be received */
            strcpy(filename, "file_list.bin");
            strcpy(te->filename, "file_list.bin");
            break;
        case GET:
            /* file name to be received */
            strcpy(filename, rqst->payload);
            strcpy(te->filename, rqst->payload);
            break;
        case PUT:
            /* file name and size to be uploaded */
            sscanf(rqst->payload, "%s\t%ld", filename, &file_len);
            strcpy(te->filename, filename);
            break;
    }
    
    /* send command to listening socket */
    if (DEBUG) printf("\rrqst sent, seq: %ld\n", (rqst->header).n_seq);
    send_packet(sockfd, rqst, sizeof(packet_t), (struct sockaddr *) &SERVADDR,
            sizeof(SERVADDR));

    /* receive answer from data socket */
    len = sizeof(new_servaddr);
    rpl = wait_reply_packet(sockfd, rqst, sizeof(packet_t),
            (rqst->header).n_seq, 
            (struct sockaddr *) &SERVADDR, sizeof(SERVADDR), 
            (struct sockaddr *) &new_servaddr, &len);

    switch ((rpl->header).type) {
        case LIST_ACK:
            if (DEBUG) printf("LIST_ACK received, seq: %ld\n", 
                    (rpl->header).n_seq);
            /* file_list size from answer pkt */
            sscanf(rpl->payload, "LIST\t%ld", &file_len);

            res = get_file(sockfd, te, (struct sockaddr *) &new_servaddr, len,
                    filename, file_len, (rpl->header).n_seq);
            
            if (res == 0) {
                /* timeout expired, trasmission cancelled */
                te->status = HU;
            } else {
                /* trasmission OK */
                print_list();
            }
            break;
        case GET_ACK:
            if (DEBUG) printf("GET_ACK received, seq: %ld\n", 
                    (rpl->header).n_seq);
            /* result of request and file size from answer pkt */
            sscanf(rpl->payload, "GET\t%s\t%ld", result, &file_len);

            if (strcmp(result, "OK") == 0 && file_len != -1) {
                /* file exists */
                res = get_file(sockfd, te, (struct sockaddr *) &new_servaddr, 
                        len, filename, file_len, (rpl->header).n_seq);
                if (res == 0) {
                    /* timeout expired, trasmission cancelled */
                    te->status = HU;
                    
                    /* build path of downloaded files */
                    strcpy(path, DOWNLOAD_DIR);
                    strcat(path, filename);
                    
                    /* deleting file */
                    abort_on_error(remove(path) < 0, "error in remove");
                }
            } else {
                /* file not found on server directory */
                te->status = NSF;
            }
            break;
        case PUT_ACK:
            if (DEBUG) printf("PUT_ACK received, seq: %ld\n", 
                    (rpl->header).n_seq);
            /* result of request from answer pkt */
            sscanf(rpl->payload, "PUT\t%s", result);

            if (strcmp(result, "OK") == 0) {
                /* upload granted */
                res = put_file(sockfd, te, (struct sockaddr *) &new_servaddr, 
                        len, filename, file_len);
                if (res == 0) {
                    /* max rtx reached, trasmission aborted */
                    te->status = HU;
                }
            } else if (strcmp(result, "FTB") == 0) {
                /* The file exceeds upload maximum file size */
                te->status = FTB;    
            }
            break;
        default:
            /* max retrasmission reached, rqst not sent */
            te->status = HU;
            break;
    }

    free(rpl);
}

int request_packet(const char *cmd, packet_t *p) {
    int fd, res;
    long n_seq;
    off_t file_len;
    char filename[MAXLINE + 1];
    const char *str;

    /* filename starts from here (after cmd type) */
    str = cmd + 4;                   
    /* ignoring spaces after the cmd */
    while (*str == ' ') str++;
    strcpy(filename, str);
    /* deleting last char (\n) */
    filename[strlen(filename) - 1] = 0;

    /* filename is empty and is not list command */
    if (strcmp(filename, "") == 0 && strncmp(cmd, "list", 4) != 0) 
        return -1;

    srand(time(NULL));
    res = 0;
    /* start n_seq is random, to solve bouncing pkt */
    n_seq = rand() % 1000; 

    /* build packet */
    if (strncmp(cmd, "get", 3) == 0) {
        (p->header).type = GET;
        (p->header).n_seq = n_seq;
        /* payload contains filename to be downloaded */
        strcpy(p->payload, filename);
        (p->header).length = strlen(p->payload);
    } else if (strncmp(cmd, "put", 3) == 0) {
        fd = open_file(filename, FILES_DIR, O_RDONLY);
        if (fd < 0) {
            /* file does not exist */
            res = 1;
        } else {
            file_len = get_file_size(fd);
            (p->header).type = PUT;
            (p->header).n_seq = n_seq;
            /* payload contains file name and size to be uploaded */
            sprintf(p->payload, "%s\t%ld", filename, file_len);
            (p->header).length = strlen(p->payload);
        }
        close(fd);
    } else if (strncmp(cmd, "list", 4) == 0) {
        (p->header).type = LIST;
        (p->header).n_seq = n_seq;
        (p->header).length = 0;
    }

    return res;
}

int check_command(const char *cmd) {
    /* cmd contains also '\n' at the end */
    if (strncmp(cmd, "get ", 4) == 0 || strncmp(cmd, "put ", 4) == 0 || 
            strcmp(cmd, "list\n") == 0)
        return 1;

    return 0;
}

packet_t *wait_reply_packet(int socket, void *rqst_pkt, size_t len, 
        long cur_nseq, struct sockaddr *dest_addr, socklen_t dest_len,
        struct sockaddr *src_addr, socklen_t *src_len) {
    packet_t *rcv_pkt;
    ssize_t nread;
    int rtx;

    /* contains received pkt */
    rcv_pkt = (packet_t *) allocate_memory(1, sizeof(packet_t));
    rtx = 0;
    for (;;) {
        nread = receive_packet(socket, rcv_pkt, sizeof(packet_t), 
                src_addr, src_len);
        if (nread < 0) {
            /* timeout event */
            if (rtx == MAX_RTX) break;

            /* request resent */
            if (DEBUG) printf("timeout: rtx rqst\n");
            send_packet(socket, rqst_pkt, len, dest_addr, dest_len);
            rtx++;
            /* double up timeout value */
            if (ADAPT) increase_timeout(socket, WINDOW_SIZE);
        /* check correct sequence number */
        } else if ((rcv_pkt->header).n_seq == cur_nseq + 1) {
            break;
        /* pkt that signal beginning of SR in get_file func */
        } else if ((rcv_pkt->header).type == DATA) {
            break;
        }
    }

    return rcv_pkt;
}

int get_file(int socket, struct thread_element *te, 
        struct sockaddr *dest_addr, socklen_t addrlen, char *filename, 
        off_t file_len, long cur_nseq) {
    int fd, outcome;
    ack_t *snd_pkt;
    packet_t *rpl;    

    if (strcmp(filename, "file_list.bin") == 0) {
        /* file overwrite */
        fd = create_file(filename, SYSTEM_DIR, 0777);
    } else {
        /* check if filename has been already used */
        acquire_mutex(&mtx3);
        fd = check_filename(filename, DOWNLOAD_DIR, 0777);
        release_mutex(&mtx3);
        strcpy(te->filename, filename);
        te->status = RUN;
    }

    /* client's ready to receive file. Build ACK packet */
    snd_pkt = (ack_t *) allocate_memory(1, sizeof(ack_t));
    (snd_pkt->header).type = ACK;
    (snd_pkt->header).n_seq = cur_nseq + 1;
 
    /* send ACK to server */
    if (DEBUG) printf("ACK sent, seq: %ld\n", (snd_pkt->header).n_seq);
    send_packet(socket, snd_pkt, sizeof(ack_t), dest_addr, addrlen);

    rpl = wait_reply_packet(socket, snd_pkt, sizeof(ack_t), (long int) -1, 
            dest_addr, addrlen, NULL, NULL);

    outcome = 0;
    if ((rpl->header).type != EMPTY) {
        /* DATA pkt received. Waiting for receiving file */
        set_timeout(socket, &TIMEOUT_CON);
        outcome = receiver_job(socket, te, dest_addr, addrlen, fd, file_len);
    }

    free(snd_pkt);
    free(rpl);
    close(fd);

    return outcome;
}

int put_file(int socket, struct thread_element *te, 
        const struct sockaddr *dest_addr, socklen_t addrlen, 
        const char *filename, off_t file_len) {
    int fd, outcome;

    fd = open_file(filename, FILES_DIR, O_RDONLY);
    te->status = RUN;
    /* trasmission starts */
    outcome = sender_job(socket, te, dest_addr, addrlen, fd, file_len);
    close(fd);

    return outcome;
}

void decode_parameters(int argc, char *argv[]) {
    int opt, res, not_addr;
    long int value;
    const char *usage = "Usage: ./client [-h IP] [-n window] "\
        "[-p probability(%)] [-t timeout(ms)] [-d] debug";

    not_addr = 1;
    /* don't want writing to stderr */
    opterr = 0; 
    while ((opt = getopt(argc, argv, "h:n:p:t:d")) != -1) {
        switch (opt) {
            case 'h':
                /* SERVADDR */
                memset((void *)&SERVADDR, 0, sizeof(SERVADDR));
                SERVADDR.sin_family = AF_INET;          
                SERVADDR.sin_port = htons(SERV_PORT);
               
                /* check ip address */
                res = inet_pton(AF_INET, optarg, &SERVADDR.sin_addr);
                abort_on_error(res < 0, "error in inet_pton");
                abort_on_error2(res == 0, "network address is not IPv4");
                
                /* -h opt typed */
                not_addr = 0;
                break;
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

    /* argument -h not typed */
    abort_on_error2(not_addr, "Error: IP address is required");
}
