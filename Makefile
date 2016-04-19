# Makefile
all: proxy
	
proxy : proxy.cpp p2.h
#	g++ -w -Wall -pthread proxy.cpp -o proxy UNCOMMENT TO IGNORE WARNINGS
	g++ -Wall -pthread proxy.cpp -o proxy