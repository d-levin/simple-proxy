P2 - Implementing a Simple HTTP Web Proxy
Dennis Levin
April 28, 2016

Files: proxy.cpp, client.cpp, p2.h, Makefile, readme.txt

Run proxy with ./proxy server_port
Run client with ./client

Proxy can be quit by signaling CTRL-C
Client runs once per execution

DESIGN & IMPLEMENTATION HIGHLIGHTS:
CLIENT:
The client attempts to connect to a server and if successful
receives finger information for the username specified
when executing ./fingerclient
The client sends the username to the server and receives
finger information.
It uses 2 helper methods to send and receive messages.

SERVER:
The server forks processes for each connection made by
a client. It supports a maximum of 10 connections as
specified by MAXCONN in p1.h
The server performs a system call to finger via execl
and redirects the output over the established socket
connection to the client.

IMPLEMENTATION DESIGN DECISION:
Limitation of 10 maximum connections can be increased
by changing constant in p1.h
Buffer size for finger response is controlled by constant
in p1.h and is currently set to 1024 bytes.
There is no error checking for incorrect input when running
the client. The client must therefor ensure that the
connection string adheres to the format specified
at the top of this readme.
 
LIMITATIONS:
None that I have found.