/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct{  // Represent a pool of connected descriptors
    int maxfd;  // Largest descriptor in read_set
    fd_set read_set;    // Set of all active descriptors
    fd_set ready_set;   // Subset of descriptors ready for reading
    int nready; // Number of ready descriptors from select
    int maxi;   // High water index into client array
    int clienfd[FD_SETSIZE];    // Set of active descriptors
    rio_t clientrio[FD_SETSIZE];    // Set of active read buffers
} pool;

typedef struct _Node{
    int ID; // stock ID
    int left_stock; // left stock count
    int price;  // stock price
    struct _Node* lchild;   // left child
    struct _Node* rchild;   // right child
}Node;

Node* root = NULL;

/* Global Variable */ 
int byte_cnt = 0;   // Counts total bytes received by server
char airplane[MAXBUF];
int airIdx = 0;

/* Function Protype */
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void init_tree();
Node* insert_tree(Node* root, int ID, int left_stock, int price);
Node* min_tree(Node* root);
Node* search_tree(Node* root, int ID);
void print_tree(Node* root);
void command(char* buf, int connfd, int n, pool* p);
int buy_stock(int ID, int cnt);
int sell_stock(int ID, int cnt);
void update(Node* root);


int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;
    
    if (argc != 2) {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(0);
    }

    init_tree();    // Initialize tree
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage); 
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }
        //printf("Connected to (%s, %s)\n", client_hostname, client_port);
    	check_clients(&pool);
    }
}
/* $end echoserverimain */


void init_pool(int listenfd, pool *p){
    /* Initially, there are no connected desciptors*/
    int i;
    p->maxi = -1;
    for(i = 0; i< FD_SETSIZE; i++){
        p->clienfd[i] = -1;
    }    

    /*Initially, listenfd is only member of select read set*/
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for(i=0; i<FD_SETSIZE; i++){
        if(p->clienfd[i] < 0){
            /*Add connected desciptor to the pool*/
            p->clienfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            /*Add the descriptor to desciptor set */
            FD_SET(connfd, &p->read_set);

            /*Update max descriptor and pool high water mark*/
            if(connfd > p->maxfd)
                p->maxfd = connfd;
            if(i> p->maxi)
                p->maxi = i;
            
            break;
        }
    }
    if(i==FD_SETSIZE)   // Couldn`t find an empty slot
        app_error("add_client error : Too many clients");

}

void check_clients(pool *p){
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0; (i<=p->maxi) && (p->nready > 0); i++){
        connfd = p->clienfd[i];
        rio = p->clientrio[i];

        /*If the descriptor is ready, echo a text line from it*/
        if((connfd > 0 ) && (FD_ISSET(connfd, &p->ready_set))){
            p->nready--;
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
                byte_cnt += n;
                printf("server received %d bytes\n", n); 
                command(buf, connfd, n, p);
                //Rio_writen(connfd, buf, n);
            }
            /* EOF detected, remove descriptor from pool*/
            else{
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clienfd[i] = -1;
            }
        }   
    }
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

void command(char* buf, int connfd, int n, pool* p ){
    int i, flag  = 1, id, cnt;
    char cmd[10], temp[MAXBUF];

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