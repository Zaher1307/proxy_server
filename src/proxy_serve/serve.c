#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "serve.h"
#include "../proxy_cache/cache.h"
#include "../safe_input_output/sio.h"
#include "../socket_interface/interface.h"


#define DEFAULT_HEADERS_SIZE 130


static const char *usr_agent_header = "User_Agent: Mozilla/5.0 (X11; Linux x86_64; rv:98.0) Gecko/20100101 Firefox/98.0\r\n";
static const char *connection_header = "Connection: close\r\n";
static const char *proxy_connection_header = "Proxy-Connection: close\r\n";


static int
parse_request_line(Sio *sio, char *method, char *url);

static int
parse_request_headers(Sio *sio, char *request_headers, char *hostname);

static void
parse_url(const char *url, char *hostname, char *port, char *uri);

static void
build_request_line(const Request *client_request, char *request_line);

static void
build_request_headers(const Request *client_request, char *request_headers);

static int
parse_response(int proxyfd, Response *server_response);

static int
parse_response_headers(Sio *sio, char *response_headers, size_t *content_length);
static void
client_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

static void
strtolwr(char *str);

int
parse_request(int clientfd, Request *client_request)
{
    Sio sio;
    char method[METHOD_LEN], url[MAX_LINE], hostname[MAX_LINE],
    port[PORT_LEN], uri[MAX_LINE], headers[MAX_BUF];

    sio_initbuf(&sio, clientfd);

    if (parse_request_line(&sio, method, url) < 0) 
        return -1;

    parse_url(url, hostname, port, uri);

    if (parse_request_headers(&sio, headers, hostname) < 0) 
        return -1;

    client_request->rq_headers = strdup(headers);
    client_request->rq_hostname = strdup(hostname);
    client_request->rq_method = strdup(method);
    client_request->rq_port = strdup(port);
    client_request->rq_uri = strdup(uri);

    return 0;
}

int
forward_request(const Request *client_request, Cache *proxy_cache,
                Response *server_response)
{
    int proxyfd, is_cached;
    char request_line[MAX_LINE], request_headers[MAX_BUF];

    build_request_line(client_request, request_line);
    build_request_headers(client_request, request_headers);

    is_cached = cache_fetch(proxy_cache, request_line, request_headers,
                &server_response->rs_line, &server_response->rs_headers,
                &server_response->rs_content, 
                &server_response->rs_content_length);
    if (!is_cached) {
        if ((proxyfd = open_clientfd(client_request->rq_hostname,
                                     client_request->rq_port)) < 0)
            return -1;

        if (sio_writen(proxyfd, request_line, strlen(request_line)) < 0)
            return -1;

        if (sio_writen(proxyfd, request_headers, strlen(request_headers)) < 0)
            return -1;

        if (parse_response(proxyfd, server_response) < 0)
            return -1;
        cache_write(proxy_cache, request_line, request_headers,
                    server_response->rs_line, server_response->rs_headers,
                    server_response->rs_content, 
                    server_response->rs_content_length);
        close(proxyfd);
    }

    return 0;
}

int
forward_response(int clientfd, const Response *server_response)
{
    if (sio_writen(clientfd, server_response->rs_line,
                strlen(server_response->rs_line)) < 0)
        return -1;
    if (sio_writen(clientfd, server_response->rs_headers,
                strlen(server_response->rs_headers)) < 0)
        return -1;
    if (sio_writen(clientfd, server_response->rs_content,
                server_response->rs_content_length) < 0)
        return -1;
    
    return 0;
}

static int
parse_request_line(Sio *sio, char *method, char *url)
{
    char request_line[MAX_LINE], version[VERSION_LEN];

    if (sio_read_line(sio, request_line, MAX_LINE) < 0)
        return -1;


    if (sscanf(request_line, "%s %s %s", method, url, version) != 3) {
        client_error(sio->sio_fd, request_line, "400", "bad request",
        "the server can't understand this request");
        return -1;
    }

    if (strcmp(method, "GET")) {
        client_error(sio->sio_fd, method, "501", "not implemented", 
        "the server doesn't implement this method");
        return -1;
    }

    if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1")) {
        client_error(sio->sio_fd, version, "505", "not supported", 
        "the server doesn't support this HTTP version");
        return -1;
    }
    
    return 0;
}

static int
parse_request_headers(Sio *sio, char *request_headers, char *hostname)
{
    ssize_t nread = MAX_BUF - DEFAULT_HEADERS_SIZE;
    char linebuf[MAX_LINE], hostnamebuf[MAX_LINE];

    /* Initialize request_headers to be ready for appending (concatination) */
    request_headers[0] = '\0';
    do {
        if (sio_read_line(sio, linebuf, MAX_LINE) < 0)
            return -1;

        if (sscanf(linebuf, "Host: %s", hostnamebuf) == 1)
            strcpy(hostname, hostnamebuf);

        strncat(request_headers, linebuf, nread);
        nread -= strlen(linebuf);
    } while (strcmp(linebuf, "\r\n") && nread > 0);


    return 0;
}

static void
parse_url(const char *url, char *hostname, char *port, char *path)
{
    char *url_copy, *url_token;

    url_copy = strdup(url);
    url_token = url_copy;

    /* Skip the "http://" part of the url */
    url_token = strstr(url, "://");
    url_token += 3;

    if (strstr(url_token, ":")) {     /* Port is contained in the url */
        /* Url token points to the hostname */
        url_token = strtok(url_token, ":");
        strcpy(hostname, url_token);
        
        /* Move url token to the port and path token */
        url_token = strtok(NULL, ":");

        if (strstr(url_token, "/")) {   /* Path is contained in the url */
            /* Parse the port from url_token */
            url_token = strtok(url_token, "/");  
            strcpy(port, url_token);

            /* Parse the path from url_token */
            url_token = strtok(NULL, "/");
            strcpy(path, url_token);
        } else {                        /* Path is not contained in the url */
            strcpy(port, url_token);
            strcpy(path, "/");  /* Set path to default */
        }

    } else {    /* Port is not contained in the url */
        strcpy(port, "80");     /* Set port to default */

        url_token = strtok(url_token, "/");
        strcpy(hostname, url_token);

        url_token = strtok(NULL, "/");

        if (url_token)          /* Path is contained in the url */
            strcpy(path, url_token);
        else                    /* Path is not contained in the url */
         strcpy(path, "/");     /* Path is set to default */
    }
    free(url_copy);
}

static void
build_request_line(const Request *client_request, char *request_line)
{
    request_line[0] = '\0';

    strcat(request_line, client_request->rq_method);
    strcat(request_line, " ");

    strcat(request_line, client_request->rq_uri);
    strcat(request_line, " ");

    strcat(request_line, "HTTP/1.0\r\n");
}

static void
build_request_headers(const Request *client_request, char *request_headers)
{
    char linebuf[MAX_LINE];

    request_headers[0] = '\0';

    /* append host request header if not appended */
    if (!strstr(client_request->rq_headers, "Host")) {
        sprintf(linebuf, "Host: %s\r\n", client_request->rq_hostname);
        strcat(request_headers, linebuf);
    }

    strcat(request_headers, usr_agent_header);
    strcat(request_headers, connection_header);
    strcat(request_headers, proxy_connection_header);
    strcat(request_headers, client_request->rq_headers);
    
    printf("%s\n", request_headers);
}

static int
parse_response(int proxyfd, Response *server_response)
{
    Sio sio;
    char response_line[MAX_LINE], response_headers[MAX_BUF];

    sio_initbuf(&sio, proxyfd);

    /* parse response line */
    if (sio_read_line(&sio, response_line, MAX_LINE) < 0)
        return -1;

    /*parse response headers */
    if (parse_response_headers(&sio, response_headers,
                &server_response->rs_content_length) < 0)
        return -1;
    
    /* parse response content */
    server_response->rs_content = malloc(server_response->rs_content_length);
    if (sio_readn(&sio, server_response->rs_content,
                server_response->rs_content_length) < 0)
        return -1;
        
    /* allocate for the data */
    server_response->rs_line = strdup(response_line);
    server_response->rs_headers = strdup(response_headers);

    return 0;
}


static int
parse_response_headers(Sio *sio, char *response_headers, size_t *content_length)
{
    char linebuf[MAX_LINE];

    /* Initialize response_headers to be ready for appending (concatination) */
    response_headers[0] = '\0';
    do {
        if (sio_read_line(sio, linebuf, MAX_LINE) < 0)
            return -1;

        strtolwr(linebuf);
        sscanf(linebuf, "content-length: %zu", content_length);
        strcat(response_headers, linebuf);
    } while (strcmp(linebuf, "\r\n"));

    return 0;
}

static void
client_error(int clientfd, char *cause, char *errnum, 
        char *short_msg, char *long_msg)
{
    char linebuf[MAX_LINE], body[MAX_BUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    strcat(body, "<body bgcolor=""ffffff"">\r\n");
    sprintf(linebuf, "%s: %s\r\n", errnum, short_msg);
    strcat(body, linebuf);
    sprintf(linebuf, "%s: %s\r\n", long_msg, cause);
    strcat(body, linebuf);

    /* Send the HTTP response */
    sprintf(linebuf, "HTTP/1.0 %s %s\r\n", errnum, short_msg);
    if (sio_writen(clientfd, linebuf, strlen(linebuf)) < 0)
        return;
    sprintf(linebuf, "Content-Type: text/html\r\n");
    if (sio_writen(clientfd, linebuf, strlen(linebuf)) < 0)
        return;
    sprintf(linebuf, "Content-Length: %zu\r\n\r\n", strlen(body));
    if (sio_writen(clientfd, linebuf, strlen(linebuf)) < 0)
        return;
    if (sio_writen(clientfd, body, strlen(body)) < 0)
        return;
}

static void
strtolwr(char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        if (isalpha(str[i]))
            str[i] = tolower(str[i]);
    }
}
