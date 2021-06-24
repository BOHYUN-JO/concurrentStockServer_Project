/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define NTHREADS 1024
#define SBUFSIZE 1024

typedef struct{
    int *buf;       /* Buffer array*/
    int n;          /* Maximum number of slots */   
    int front;      /* buf[(front+1)%n] is first item*/
    int rear;       /* buf[rear%n] is last item*/
    sem_t mutex;    /* Protects accesses to buf*/
    sem_t slots;    /* Counts available slots*/
    sem_t items;    /* Counts available items */
}sbuf_t;

typedef struct _Node{
    int ID; // stock ID
    int left_stock; // left stock count
    int price;  // stock price
    struct _Node* lchild;   // left child
    struct _Node* rchild;   // right child
}Node;

Node* root = NULL;

/* Global Variable */ 
static int byte_cnt = 0;   // Counts total bytes received by server
static sem_t mutex;
char airplane[MAXBUF];
int airIdx = 0;
sbuf_t sbuf;

/* Function Protype */
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
void* thread(void *vargp);
static void init_echo_cnt(void);
void init_tree();
Node* insert_tree(Node* root, int ID, int left_stock, int price);
Node* search_tree(Node* root, int ID);
void print_tree(Node* root);
void command(int connfd);
int buy_stock(int ID, int cnt);
int sell_stock(int ID, int cnt);
void update(Node* root);


int main(int argc, char **argv) 
{
    int listenfd, connfd,i;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;
    
    if (argc != 2) {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(0);
    }

    init_tree();    // Initialize tree
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    for(i=0; i< NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        
    }
    
}
/* $end echoserverimain */

/*Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                          /* Buffer holds max of n items */
    sp->front = sp->rear = 0;           /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0 , 1);        /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);         /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);         /* Initially, buf has 0 items */
}

/* Clean up buffer sp*/
void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp*/
void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

/* Remove and return the first item from buffer sp*/
int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}


void*thread(void*vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        //echo_cnt(connfd);
        command(connfd);
        Close(connfd);
    }

}

static void init_echo_cnt(void){
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

void init_tree(){
    FILE* fp;
    if( (fp = fopen("stock.txt", "r")) == NULL){    // File Error check
        printf("error: can not open file\n");
    }
    while(!feof(fp)){   // Get information from stock.txt
        int ID, left_stock, price;
        fscanf(fp, "%d %d %d", &ID, &left_stock, &price);            
        root = insert_tree(root, ID, left_stock, price);
    }
    fclose(fp);
}

Node* insert_tree(Node* root, int ID, int left_stock, int price){
    if(root == NULL){   // Empty tree
        root = (Node*)malloc(sizeof(Node));
        root->rchild = root->lchild = NULL;
        root->ID = ID;
        root->left_stock = left_stock;
        root->price = price;
        return root;
    }else if(root->ID == ID){
        // Nothing to do - same value is not allowed
    }
    else{
        if(ID < root->ID){  // Left child
            root->lchild = insert_tree(root->lchild, ID, left_stock, price);
        }else{  // Right child
            root->rchild = insert_tree(root->rchild, ID, left_stock, price);
        }
    }

    return root;
}

void print_tree(Node* root){
    char buf[MAXBUF];
    if(root != NULL){
        print_tree(root->lchild);   // Left child
        sprintf(buf, "%d %d %d|", root->ID, root->left_stock, root->price );
        strcat(airplane, buf);
        print_tree(root->rchild);   // Right child 
    }
}

Node* search_tree(Node* root, int ID){
    if(root == NULL){   // Can not find
        return NULL;
    }else if(root->ID == ID){   // Find
        return root;
    }else if(root->ID < ID){
        search_tree(root->rchild, ID);
    }else{
        search_tree(root->lchild, ID);
    }
}

void command(int connfd){
    int i, flag  = 1, id, cnt,n;
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    char cmd[10], buf[MAXBUF], temp[MAXBUF];
    
    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        P(&mutex);
        byte_cnt += n;
        printf("server received %d bytes\n", n);
        for(i=0; i<n ;i++){
            if(buf[i] == ' '){
               flag = 0;
            } 
        }

        if(flag){   // Ex) show, exit
            sscanf(buf, "%s", cmd);
        }else{  // Ex) buy 1 3, sell 2 5
            sscanf(buf, "%s %d %d", cmd, &id, &cnt);
        }

        if(strcmp(cmd, "show") == 0){   // Print stock list 
            airplane[0] = '\0';
            print_tree(root);
            airplane[strlen(airplane)-1] = ' ';
            strcat(airplane, "\n");
            Rio_writen(connfd, airplane, strlen(airplane));
        }else if(strcmp(cmd, "buy") == 0){  // Buy some stock
            if( buy_stock(id, cnt)){    // Can buy stock
                strcpy(temp, "[buy] success");
                strcat(temp, "\n");
               
                Rio_writen(connfd, temp, strlen(temp));
                update(root);   // Update stock table
            }else{  // Can`t buy stock
                strcpy(temp, "Not enough left stocks");
                strcat(temp, "\n");
                Rio_writen(connfd, temp, strlen(temp));
            }
        }else if(strcmp(cmd, "sell") == 0){ // Sell some stock
            if(sell_stock(id, cnt)){    
                strcpy(temp, "[sell] success");
                strcat(temp, "\n");
                Rio_writen(connfd, temp, strlen(temp));
                update(root);   // update stock table
            }
        }
        V(&mutex);
    }
 
}

int buy_stock(int ID, int cnt){
    Node* ptr = search_tree(root, ID);
    if( ptr->left_stock - cnt >= 0){    // Enough left stock
        ptr->left_stock -= cnt;
        return 1;
    }else{  // Not enough left stock
        return 0;
    }
}

int sell_stock(int ID, int cnt){
    Node* ptr = search_tree(root, ID);
    ptr->left_stock += cnt; 
    return 1;
}

void update(Node* root){
    /*update stock table*/
    int i;
    FILE* fp = fopen("stock.txt", "w");
    airplane[0] = '\0';
    print_tree(root);
    airplane[strlen(airplane)-1] = ' ';
    for(i=0; i<strlen(airplane); i++){
        if(airplane[i] == '|'){
            airplane[i] ='\n';
        }
    } 
    fprintf(fp, "%s", airplane);
    fclose(fp);

}