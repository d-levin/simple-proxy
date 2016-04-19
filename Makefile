# Makefile
all: proxy
	
proxy : proxy.cpp p2.h
	g++ -Wall -pthread proxy.cpp -o proxy
