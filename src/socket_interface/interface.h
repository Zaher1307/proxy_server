#ifndef INTERFACE_H
#define INTERFACE_H

int
open_clientfd(char *hostname, char *port);

int
open_listenfd(char *port);

#endif