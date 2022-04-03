#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proxy_cache/cache.h"
#include "proxy_serve/serve.h"
#include "socket_interface/interface.h"

#define safe_free(ptr) sfree((void **) &(ptr))

typedef struct sockaddr SA;
typedef struct vargp {
    int clientfd;
    Cache *proxy_cache;
} Vargp;

static void*
client_serve(void* vargp);

static void
sfree(void **ptr);

static void
free_resources(Request *client_request, Response *server_response);

int 
main(int argc, char **argv)
{
    int connfd, listenfd;
    char hostname[MAX_LINE], port[PORT_LEN];
    socklen_t client_len;
    struct sockaddr_storage client_addr;
    pthread_t tid;
    Cache proxy_cache;
    Vargp *vargp;

    signal(SIGPIPE, SIG_IGN);

    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = open_listenfd(argv[1]);
    cache_init(&proxy_cache);

    while (1) {
        client_len = sizeof(client_addr);
        if ((connfd = accept(listenfd, (SA* ) &client_addr, &client_len)) < 0) {
            fprintf(stderr, "Connection to (%s, %s) failed\n", hostname, port);
            continue;
        }
        vargp = malloc(sizeof(Vargp));
        vargp->clientfd = connfd;
        vargp->proxy_cache = &proxy_cache;
        pthread_create(&tid,NULL, client_serve, vargp); 
    }
}

static void*
client_serve(void *vargp)
{
    int clientfd;
    Request client_request;
    Response server_respone;
    Cache *proxy_cache;

    clientfd = ((Vargp *)vargp)->clientfd;
    proxy_cache = ((Vargp *)vargp)->proxy_cache;

    /* freeing the allocated memeory that vargp points to */
    safe_free(vargp); 

    /* detach the thread after ending its job */
    pthread_detach(pthread_self());

    /* initialize client_request and server_response resources with zero */
    memset(&client_request, 0, sizeof(Request));
    memset(&server_respone, 0, sizeof(Response));

    /* parse HTTP request */
    if (!(parse_request(clientfd, &client_request) < 0)) {
        /* forward the request to the server if it parsed successfully */
        if (!(forward_request(&client_request, proxy_cache,
                              &server_respone) < 0)) {
            /* forward server response to the client after requesting 
             *  successfully */
            if (!(forward_response(clientfd, &server_respone) < 0)) {
                /* code region if any dependent action after successfull 
                 * server_respone */
            }
        }
    }

    free_resources(&client_request, &server_respone);
    close(clientfd);
    return NULL;
}

static void
sfree(void **ptr)
{
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}


static void
free_resources(Request *client_request, Response *server_response)
{
    safe_free(client_request->rq_headers);
    safe_free(client_request->rq_hostname);
    safe_free(client_request->rq_method);
    safe_free(client_request->rq_port);
    safe_free(client_request->rq_uri);

    safe_free(server_response->rs_content);
    safe_free(server_response->rs_headers);
    safe_free(server_response->rs_line);
}
