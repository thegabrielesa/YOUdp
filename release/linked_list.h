typedef struct node {
    packet_t pkt;
    struct node *next;
} node_t;


/* prototypes */
int is_empty(node_t *head);
void print(node_t *head);
void insert(node_t **head, packet_t *new_pkt);
void push(node_t **head, node_t *new_node);
node_t *pop(node_t **head);


/******************************************************************************
**                                                                           **
**                                   Methods                                 **
**                                                                           **
******************************************************************************/
int is_empty(node_t *head) {
    return (head == NULL);
}

void print(node_t *head) {
    node_t *curr;

    if (is_empty(head)) {
        printf("Empty list!\n");
        return;
    }

    printf("| ");
    for (curr = head; curr != NULL; curr = curr->next)
        printf("%ld | ", (curr->pkt).header.n_seq);
    printf("\n");
}

void insert(node_t **head, packet_t *new_pkt) {
    node_t *new_node, *curr, *prev;

    /* scan list until new sequence number is less than the current ones */
    for(curr = *head, prev = NULL; curr != NULL && 
            (curr->pkt).header.n_seq < (new_pkt->header).n_seq; 
            prev = curr, curr = curr->next)
        ;

    /* if sequence number is already there, discard packet */
    if (curr != NULL && (curr->pkt).header.n_seq == (new_pkt->header).n_seq) 
        return;

    /* store new node */
    new_node = malloc(sizeof(node_t));
    if (new_node == NULL) {
        perror("error in malloc");
        exit(EXIT_FAILURE);
    }
    memcpy(&(new_node->pkt), new_pkt, sizeof(packet_t));
    new_node->next = curr;

    /* first node */
    if (prev == NULL) *head = new_node;
    else prev->next = new_node;
}

void push(node_t **head, node_t *new_node) {
    /* insert as first */
    new_node->next = *head;
    *head = new_node;
}

node_t *pop(node_t **head) {
    node_t *temp;

    if (is_empty(*head)) return NULL;

    /* get the first node */
    temp = *head;
    *head = (*head)->next;
    return temp;
}
