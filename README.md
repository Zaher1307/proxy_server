Proxy Server<a name="TOP"></a>
=============================
A Web proxy is a program that acts as a middleman between a Web browser and an 
end server. Instead of contacting the end server directly to get a Web page, 
the browser contacts the proxy, which forwards the request on to the end server. 
When the end server replies to the proxy, the proxy sends the reply on to the 
browser.

## Discription
- It's the last lab of [15-213: Introduction to Computer Systems](https://www.cs.cmu.edu/afs/cs.cmu.edu/academic/class/15213-f15/www/schedule.html).
- It's a simple `concurrent` HTTP proxy that handles `HTTP/1.0` `GET` requests and 
  `caches` recently-requested web objects.
- It handles multiple connections at the same time using `multi-threading` 
  programing.
- It `caches` web objects by storing a copy in disk to respond to future 
  connections by reading them from cache if the request already exist.
  It uses least recently used eviction policy.

**NOTE:** All project specifications and usege are mentioned in this
[writeup](https://github.com/Zaher1307/proxy_server/blob/master/proxylab.pdf)


## How does this Proxy work?
1) **The proxy accepts a connection form a client then add this client to a 
   thread.**
2) **Then it read the request line and request headers from this client, if any
   problem it tell the client there is an error and end the connection, 
   otherwise it uses this request.**
3) **If this is a good request, it search in the cache for the content fo this
   request then forwards it to the client.**
4) **If this request is not cached then the proxy opens a connection with the
   server and get response from server and caches the content, then forwards it
   to the client.**

## How does this Proxy implemented?
It is consisted of some modules with the following details:

- [`safe_input_output:`](https://github.com/Zaher1307/proxy_server/tree/master/src/safe_input_output)
  this module is responsible for providing a safe reentrant routines to read from and 
  write to connection sockets without short count.
- [`socket_interface:`](https://github.com/Zaher1307/proxy_server/tree/master/src/socket_interface)
  this module is responsible for providing a routines for openning and requesting
  a connection as a user and routines for listenning to connection as a server.
- [`proxy_cahce:`](https://github.com/Zaher1307/proxy_server/tree/master/src/proxy_cache)
  this module is responsible for caching web objects, choosing the victim set to 
  evict and updating all cache.
- [`proxy_serve:`](https://github.com/Zaher1307/proxy_server/tree/master/src/proxy_serve)
    this module is responsible for serving the client after accepting the
    the connection with the following sequence:


    - Reads the the request line and headers.
    - Parses the request line to get method, url and http version.
      - prase the url to get host name, uri and port number.
    - Parses the request headers to get the headers.
    - Searh in the cache if that request is already exist in the cache then
      forwards the content to the user and end. If not it continues to the next
      step.
    - Build the new request line with HTTP/1.0 version.
    - Build the new request headers with needed headers that mentioned in the
      [writeup](https://github.com/Zaher1307/proxy_server/blob/master/proxylab.pdf).
    - Open a connection with the web server then get the response and cache it
      then forwards it to the user.

    <br/>

    *If there is a problem in any of the previous steps, the proxy tell the 
    client then end the connection.*

- [`proxy.c:`](https://github.com/Zaher1307/proxy_server/blob/master/src/proxy.c)
  this is the main program that open listen for a connection request to our
  proxy then put the client in a thread to be served.

## Requirements
- `linux`
- `git`
- `gcc`
- `make`

## How to use this Proxy?
1) **Clone, compile and run the source code as following:**

     ``` 
      git clone https://github.com/Zaher1307/proxy_server
     ``` 

     ``` 
      make
     ``` 

     ``` 
      ./proxy <port>
     ``` 

2) **Send and HTTP request to the server using**

    *telnet:*

     ``` 
     telnet localhost <port>
     GET http://www.example.com HTTP/1.0
     ``` 

    or you can clone and send request to my http [`tiny_web_server`](https://github.com/Zaher1307/tiny_web_server)
    instead of http websites.

    *browser:*
 
     ``` 
     http://www.example.com
     ``` 



