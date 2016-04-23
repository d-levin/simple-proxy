# Makefile
all: proxy
	
proxy : proxy.cpp proxy.h
	g++ -g3 -Wall -pthread proxy.cpp -o proxy