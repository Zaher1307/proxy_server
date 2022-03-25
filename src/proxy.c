#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proxy_serve/serve.h"
#include "socket_interface/interface.h"

#define safe_free(ptr) sfree((void **) &(ptr))

typedef struct sockaddr SA;

static void
client_serve(int connfd);

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

    signal(SIGPIPE, SIG_IGN);

    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = open_listenfd(argv[1]);

    while (1) {
        client_len = sizeof(client_addr);
        if ((connfd = accept(listenfd, (SA* ) &client_addr, &client_len)) < 0) {
            fprintf(stderr, "Connection to (%s, %s) failed\n", hostname, port);
            continue;
        }
        client_serve(connfd);
        close(connfd);
    }
}

static void
client_serve(int connfd)
{
    Request client_request;
    Response server_respone;

    /* initialize client_request and server_response resources with zero */
    memset(&client_request, 0, sizeof(Request));
    memset(&server_respone, 0, sizeof(Response));

    if (
    /* parse HTTP request */
    parse_request(connfd, &client_request) < 0 ||
    /* forward the request to the server if it parsed successfully */
    forward_request(&client_request, &server_respone) < 0 ||
    /* forward server response to the client after requesting successfully */
    forward_response(connfd, &server_respone) < 0) {
        /* if any problem in the serving then free resourses then return */
        free_resources(&client_request, &server_respone);
        return;
    }

    free_resources(&client_request, &server_respone);
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