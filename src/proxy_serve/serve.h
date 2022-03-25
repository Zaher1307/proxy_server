#ifndef SERVE_H
#define SERVE_H

#include <sys/types.h>

#define MAX_LINE    8192        /* 8KB line buffer */
#define MAX_BUF     1048576     /* 1MB buffer size */
#define PORT_LEN    10          /* 10B port length */
#define VERSION_LEN 10          /* 10B http version length */
#define METHOD_LEN  10          /* 10B method length */

typedef struct request {
    char *rq_method;
    char *rq_hostname;
    char *rq_port;
    char *rq_uri;
    char *rq_headers;
} Request;

typedef struct response {
    char *rs_line;
    char *rs_headers;
    void *rs_content;
    size_t  rs_content_length;
} Response;

int
parse_request(int clientfd, Request *client_request);

int
forward_request(const Request *client_request, Response *server_response);

int
forward_response(int clientfd, const Response *server_response);

#endif