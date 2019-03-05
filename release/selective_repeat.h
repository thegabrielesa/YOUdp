#include "linked_list.h"

/* prototypes */
/* sender methods */
int sender_job(int socket, struct thread_element *te, 
        const struct sockaddr *dest_addr, socklen_t addrlen, 
        int fd, off_t file_len);
int send_pkt_in_buffer(int sockfd, struct circular_buffer *buffer, 
        const struct sockaddr *dest_addr, socklen_t addrlen);
int receive_ack(int sockfd, struct circular_buffer *buffer, long *sb, 
        const struct sockaddr *dest_addr, socklen_t addrlen);
int retransmit_packets(int sockfd, struct circular_buffer *buffer,
        const struct sockaddr *dest_addr, socklen_t addrlen);
void set_ack(struct circular_buffer *buffer, long num_pkt);
int move_window(struct circular_buffer *buffer);
/* cumulative timeout methods */
void increase_timeout(int socket, int lost_pkt);
void decrease_timeout(int socket, int rcv_ack);
/* receiver methods */
int receiver_job(int socket, struct thread_element *te, 
        const struct sockaddr *dest_addr, socklen_t addrlen, 
        int fd, off_t file_size);
void send_ack(int sockfd, long n_pkt, const struct sockaddr *dest_addr, 
        socklen_t addrlen);
/* I/O methods */
int read_file(int fd, struct circular_buffer *buffer, long *nsq, long num_pkt);
int write_file(int fd, node_t **list, long index);


/******************************************************************************
**                                                                           **
**                              sender methods                               **
**                                                                           **
******************************************************************************/
int sender_job(int socket, struct thread_element *te, 
        const struct sockaddr *dest_addr, socklen_t addrlen, 
        int fd, off_t file_len) {
    int acked, rtx;
    ssize_t nread;
    long send_base, nextseqnum, num_pkt;
    struct circular_buffer *buffer;
    ack_t *snd_pkt, *rcv_pkt;

    /* total pkt to send */
    num_pkt = file_len / ((long) DIM_PAYLOAD);
    if (file_len % DIM_PAYLOAD != 0) num_pkt++;
    if (DEBUG) printf("num_pkt to send: %ld\n", num_pkt);

    /* allocate enough memory to store all the pkt of the window */
    buffer = (struct circular_buffer *) allocate_memory(1, 
            sizeof(struct circular_buffer));
    buffer->window = (packet_t *) allocate_memory(BUFFER_SIZE, 
            sizeof(packet_t));
    buffer->acked = (int *) allocate_memory(BUFFER_SIZE, sizeof(int));

    /* progress info of trasmission */
    if (te != NULL) {
        te->tot_pkt = num_pkt;
        gettimeofday(&(te->start), NULL);
    }

    /* SR starts */
    send_base = 0;
    nextseqnum = 0;
    acked = 0;
    while (send_base < num_pkt) {
        /* if having pkt to send */
        if (nextseqnum < num_pkt) {
            read_file(fd, buffer, &nextseqnum, num_pkt);
            send_pkt_in_buffer(socket, buffer, dest_addr, addrlen);
        }
        acked = receive_ack(socket, buffer, &send_base, dest_addr, addrlen);
        /* timeout, max retransmission */
        if (!acked) break;
        /* updating progress info */
        if (te != NULL) {
            te->cur_pkt = send_base; 
            gettimeofday(&(te->now), NULL); 
        }
    }

    if (acked) {
        /* sent all pkts and acked */
        snd_pkt = (ack_t *) allocate_memory(1, sizeof(ack_t));
        (snd_pkt->header).type = FIN;
        (snd_pkt->header).n_seq = num_pkt;
        /* sending FIN */
        if (DEBUG) printf("FIN sent, seq = %ld\n", (snd_pkt->header).n_seq);
        send_packet(socket, snd_pkt, sizeof(ack_t), dest_addr, addrlen);

        rcv_pkt = (ack_t *) allocate_memory(1, sizeof(ack_t));
        rtx = 0;
        for (;;) {
            /* waiting FINACK */
            nread = receive_packet(socket, rcv_pkt, sizeof(ack_t), NULL, NULL);
            if (nread < 0) {
                /* timeout */
                if (rtx == MAX_RTX) {
                    if (DEBUG) printf("FIN not acked\n");
                    break;
                }
                /* rtx FIN */
                if (DEBUG) printf("timeout: FIN sent again\n");
                send_packet(socket, snd_pkt, sizeof(ack_t), 
                        dest_addr, addrlen);
                rtx++;
            } else if ((rcv_pkt->header).type == FINACK &&
                    (rcv_pkt->header).n_seq == num_pkt + 1) {
                if (DEBUG) printf("FINACK received, seq = %ld\n",
                        (rcv_pkt->header).n_seq);
                break;
            }
        }
        free(snd_pkt);
        free(rcv_pkt);
    }

    free(buffer->acked);
    free(buffer->window);
    free(buffer);
    close(fd);

    return acked;
}

int send_pkt_in_buffer(int sockfd, struct circular_buffer *buffer,
		const struct sockaddr *dest_addr, socklen_t addrlen) {
    packet_t *pkt;
    int nsent;

    nsent = 0;
    for (;;) {
        /* no new pkt to send */
        if (buffer->S == buffer->E) break;

        pkt = &(buffer->window[buffer->S]);
        if (DEBUG) printf("pkt%ld sent\n", (pkt->header).n_seq);
        send_packet(sockfd, pkt, sizeof(packet_t), dest_addr, addrlen);
        
        buffer->S = (buffer->S + 1) % BUFFER_SIZE;
        nsent++;
    }
    return nsent;
}

int receive_ack(int sockfd, struct circular_buffer *buffer, long *sb, 
        const struct sockaddr *dest_addr, socklen_t addrlen) {
    ssize_t nread;
    int acked, sent;
    ack_t *rcv_pkt;
    int rtx;

    rcv_pkt = (ack_t *) allocate_memory(1, sizeof(ack_t));
    rtx = 0;
    acked = 0;
    for (;;) {    
        nread = receive_packet(sockfd, rcv_pkt, sizeof(ack_t), NULL, NULL);
        if (nread < 0) {
            /* timeout */
            if (rtx == MAX_RTX) {
                acked = 0;
                break;
            }
            /* rtx all not acked pkts in window */
            sent = retransmit_packets(sockfd, buffer, dest_addr, addrlen);
            rtx++;
            if (ADAPT) increase_timeout(sockfd, sent);
        } else if ((rcv_pkt->header).type == ACK) {
            /* received ack pkt */
            if ((rcv_pkt->header).n_seq >= *sb && 
                    (rcv_pkt->header).n_seq < *sb + WINDOW_SIZE) {
                /* received ack for pkt in window */
                if (DEBUG) printf("ack%ld received\n", 
                        (rcv_pkt->header).n_seq);
                /* setting pkt as received */
                set_ack(buffer, (rcv_pkt->header).n_seq);
                acked++;
                /* reset rtx counter */
                rtx = 0;

                if ((rcv_pkt->header).n_seq == *sb) {
                    /* move window's base of consecutive ack rcved from sb */
                    *sb += move_window(buffer);
                    if (ADAPT) decrease_timeout(sockfd, acked);
                    /* there're free place in buffer for new pkts to send */
                    break;
                }
            }
        }
    }
    free(rcv_pkt);

    return acked;
}

void set_ack(struct circular_buffer *buffer, long num_pkt) {
    int nN;
    packet_t *pkt;

    /* position older pkt not acked */
    nN = buffer->N;
    for (;;) {
        pkt = &(buffer->window[nN]);
        /* find the relative pkt */
        if ((pkt->header).n_seq == num_pkt) {
            buffer->acked[nN] = 1;
            break;
        }

        nN = (nN + 1) % BUFFER_SIZE;
        /* visited all sent pkts */
        if (nN == buffer->E) break;
    }
}

int move_window(struct circular_buffer *buffer) {
    int acked;

    acked = 0;
    for (;;) {
        /* until find consecutive ack or visited all pkts */
        if (buffer->acked[buffer->N] == 0 || buffer->N == buffer->E) break;

        /* reset */
        buffer->acked[buffer->N] = 0;
        buffer->N = (buffer->N + 1) % BUFFER_SIZE;
        acked++;
    }
    return acked;
}

int retransmit_packets(int sockfd, struct circular_buffer *buffer,  
        const struct sockaddr *dest_addr, socklen_t addrlen) {
    int nN, sent;
    packet_t *pkt;

    /* position older pkt not acked */
    nN = buffer->N;
    sent = 0;
    for (;;) {
        /* pkt not alredy acked */
        if (buffer->acked[nN] == 0) {
            pkt = &(buffer->window[nN]);
            if (DEBUG) printf("timeout: pkt%ld sent again\n", 
                    (pkt->header).n_seq);
            send_packet(sockfd, pkt, sizeof(packet_t), dest_addr, addrlen);
            sent++;
        }

        nN = (nN + 1) % BUFFER_SIZE;
        /* all sent packet are visited */
        if (nN == buffer->E) break;
    }

    return sent;
}


/******************************************************************************
**                                                                           **
**                         cumulative timeout methods                        **
**                                                                           **
******************************************************************************/
void increase_timeout(int socket, int lost_pkt) {
    double lost_perc;
    long int ms;
    struct timeval timeout;

    lost_perc = (double) lost_pkt / WINDOW_SIZE;
    /* if loss in window > 20%, timeout increasing */
    if (lost_perc > 0.2) {
        get_timeout(socket, &timeout);

        /* current timeout in ms */
        ms = timeout.tv_sec * 1000 + timeout.tv_usec / 1000;
        /* new timeout = current timeout * (%lost + 1) */
        ms = ((ms * (lost_perc + 1)) > T_MAX) ? T_MAX : (ms * (lost_perc + 1));
        if (DEBUG) {
            printf("********************\n");
            printf("* %%lost=%3.2f       *\n", lost_perc);
            printf("* new_timeout=%-4ld *\n", ms);
            printf("********************\n");
        }
        timeout.tv_sec = ms / 1000;
        timeout.tv_usec = (ms % 1000) * 1000;

        set_timeout(socket, &timeout);
    }
}

void decrease_timeout(int socket, int rcv_ack) {
    double rcv_perc;
    long int ms;
    struct timeval timeout;

    rcv_perc = (double) rcv_ack / WINDOW_SIZE;
    get_timeout(socket, &timeout);
    
    /* current timeout in ms */
    ms = timeout.tv_sec * 1000 + timeout.tv_usec / 1000;
    /* new timeout = current timeout / (%rcv + 1) */
    ms = ((ms / (rcv_perc + 1)) < T_MIN) ? T_MIN : (ms / (rcv_perc + 1));
    if (DEBUG) {
        printf("********************\n");
        printf("* %%rcv=%3.2f        *\n", rcv_perc);
        printf("* new_timeout=%-4ld *\n", ms);
        printf("********************\n");
    }
    timeout.tv_sec = ms / 1000;
    timeout.tv_usec = (ms % 1000) * 1000;
    
    set_timeout(socket, &timeout);
}


/******************************************************************************
**                                                                           **
**                            receiver methods                               **
**                                                                           **
******************************************************************************/
int receiver_job(int socket, struct thread_element *te, 
        const struct sockaddr *dest_addr, socklen_t addrlen, 
        int fd, off_t file_size) {
    int outcome;
    ssize_t nread;
    long rcv_base, num_pkt;
    packet_t *rcv_pkt;
    ack_t *snd_pkt;
    node_t *linked_list = NULL;
    
    /* total pkt to receive */
    num_pkt = file_size / ((long) DIM_PAYLOAD);
    if (file_size % DIM_PAYLOAD != 0) num_pkt++;
    if (DEBUG) printf("num_pkt to receive: %ld\n", num_pkt);

    /* progress info of trasmission */
    if (te != NULL) {
        te->tot_pkt = num_pkt;
        gettimeofday(&(te->start), NULL);
    }

    rcv_pkt = (packet_t *) allocate_memory(1, sizeof(packet_t));
    /* SR starts */
    rcv_base = 0;
    for (;;) {    
        nread = receive_packet(socket, rcv_pkt, sizeof(packet_t), NULL, NULL);
        if (nread < 0) {
            /* timeout (connection) */
            outcome = 0;
            break;
        } else if ((rcv_pkt->header).type == DATA) {
            /* received data pkt */
            if ((rcv_pkt->header).n_seq >= rcv_base &&
                    (rcv_pkt->header).n_seq < rcv_base + WINDOW_SIZE) {
                /* its sequence number falls into the window
                   insert pkt into linked list */
                insert(&linked_list, rcv_pkt);

                if (DEBUG) printf("pkt%ld received\t", 
                        (rcv_pkt->header).n_seq);
                send_ack(socket, (rcv_pkt->header).n_seq, dest_addr, addrlen);

                if ((rcv_pkt->header).n_seq == rcv_base) {
                    /* move window's base of consecutive pkt received from rb
                       and write their payload in the file */
                    rcv_base += write_file(fd, &linked_list, 
                            (rcv_pkt->header).n_seq);

                    /* update progress info */
                    if (te != NULL) {
                        te->cur_pkt = rcv_base; 
                        gettimeofday(&(te->now), NULL); 
                    }
                }
            } else if ((rcv_pkt->header).n_seq >= rcv_base - WINDOW_SIZE &&
                    (rcv_pkt->header).n_seq < rcv_base) {
                /* its sequence number falls into the previous window */
                if (DEBUG) printf("pkt%ld received out of sequence\t",
                        (rcv_pkt->header).n_seq);
                send_ack(socket, (rcv_pkt->header).n_seq, dest_addr, addrlen);
            } else {
                /*discarding pkt */
                ;
            }
        } else if ((rcv_pkt->header).type == FIN && 
                (rcv_pkt->header).n_seq == num_pkt) {
            /* sender has sent all pkts ==> joint closure */
            if (DEBUG) printf("FIN received, seq = %ld\n", 
                    (rcv_pkt->header).n_seq);
            
            snd_pkt = (ack_t *) allocate_memory(1, sizeof(ack_t));
            (snd_pkt->header).type = FINACK;
            (snd_pkt->header).n_seq = num_pkt + 1;

            if (DEBUG) printf("FINACK sent, seq = %ld\n", 
                    (snd_pkt->header).n_seq);
            send_packet(socket, snd_pkt, sizeof(ack_t), dest_addr, addrlen);

            outcome = 1;
            free(snd_pkt);
            break;
        }
    }
    free(rcv_pkt);    

    return outcome;
}

void send_ack(int sockfd, long n_pkt, const struct sockaddr *dest_addr, 
        socklen_t addrlen) {
    ack_t *snd_pkt;

    /* build pkt */
    snd_pkt = (ack_t *) allocate_memory(1, sizeof(ack_t));
    (snd_pkt->header).type = ACK;
    (snd_pkt->header).n_seq = n_pkt;

    if (DEBUG) printf("ack%ld sent\n", (snd_pkt->header).n_seq);
    send_packet(sockfd, snd_pkt, sizeof(ack_t), dest_addr, addrlen);

    free(snd_pkt);
}


/******************************************************************************
**                                                                           **
**                               I/O methods                                 **
**                                                                           **
******************************************************************************/
int read_file(int fd, struct circular_buffer *buffer, long *nsq, long num_pkt){
    int nread, nE;
    ssize_t nleft;
    packet_t *pkt;

    nread = 0;
    for (;;) {
        nE = (buffer->E + 1) % BUFFER_SIZE;

        /* full buffer */
        if (nE == buffer->N) break;

        pkt = &(buffer->window[buffer->E]);
        /* build packet */
        (pkt->header).type = DATA;
        (pkt->header).n_seq = (*nsq)++;
        nleft = full_read(fd, pkt->payload, DIM_PAYLOAD);
        abort_on_error(nleft < 0, "error in full_read");
        (pkt->header).length = DIM_PAYLOAD - nleft;

        nread++;
        buffer->E = nE;

        /* all pkts sent */
        if (*nsq >= num_pkt) break;
    }

    return nread;
}

int write_file(int fd, node_t **list, long index) {
    int nwritten;
    ssize_t nleft;
    long i;
    packet_t *pkt;
    node_t *node;

    nwritten = 0;
    /* start sequence number */
    i = index;
    for (;;) {
        node = pop(list);
        /* empty list */
        if (node == NULL) break;
        pkt = &(node->pkt);

        if ((pkt->header).n_seq == i) {
            /* pkt taken is consecutive */
            nleft = full_write(fd, pkt->payload, (pkt->header).length);
            abort_on_error(nleft < 0, "error in full_write");
            nwritten++;
            i++;
            free(node);
        } else {
            /* not consecutive, reinsert on list */
            push(list, node);
            break;
        }
    }

    return nwritten;
}
