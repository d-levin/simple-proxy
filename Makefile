# Makefile
all: proxy
	
proxy : proxy.cpp p2.h
	g++ -g3 -Wall -pthread proxy.cpp -o proxy