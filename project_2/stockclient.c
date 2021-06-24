/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd, i, n;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
	fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        if(strncmp(buf, "exit",4) == 0){
            close(clientfd);
            exit(1);
        }
	    Rio_writen(clientfd, buf, strlen(buf));
	    if( (n = Rio_readlineb(&rio, buf, MAXLINE)) != 0 ){ 
            for(i=0; i<=n; i++){
                if(buf[i] == '|'){
                    buf[i] = '\n';
                }
            }
            Fputs(buf, stdout);
        }
    }

    exit(0);
}
/* $end echoclientmain */
