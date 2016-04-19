/*
 *    Filename: client.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include "p2.h"
using namespace std;

// Handles CTRL-C signals
void termination_handler(int signum) {
	printf("Terminating program...\n");
	done = 1;
	exit(EXIT_SUCCESS);
}

// Send a message to given connection
void sendMsg(int s, int size, int sent, char *ptr) {
	while ((size > 0) && (sent = write(s, ptr, size))) {
		// Check error
		if (sent < 0) {
			error("Write failed");
		}

		// Move pointer forward in struct by size of sent data
		ptr += sent;

		// Keep track of how much data has been sent
		size -= sent;
	}
}

// Receive a message from given connection
void receiveMsg(int s, int size, int received, char *ptr) {
	while ((size > 0) && (received = read(s, ptr, size))) {
		// Check error
		if (received < 0)
			error("Read failed");

		// Move pointer forward in struct by size of sent data
		ptr += received;

		// Keep track of how much data has been sent
		size -= received;
	}
}

// Setup connection and send/receive
void conn(char *argv[]) {
	// Create struct and zero out
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));

	// Check host name
	struct hostent *host = gethostbyname(argv[SERVER]);
	if (!host) {
		error("Could not resolve host name");
	}

	// Copy host name to struct
	memcpy(&sa.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

	// Set struct properties
	int port = atoi(argv[CLIENT_PORT]);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);

	// Open socket
	int s = socket(AF_INET, SOCK_STREAM, 0);

	// Socket failed
	if (s < 0) {
		error("Failed to open socket");
	}

	// Connect to host
	if (connect(s, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
		error("Connection failed");
	} else if (DEBUG) {
		printf("Connected to server!\n");
	}

	// Cleanup
	close(s);
}

// Driver
int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 3) {
		printf("Program needs more arguments\n");
		exit(EXIT_SUCCESS);
	}

	// Handler
	// Quit on CTRL-C
	signal(SIGINT, termination_handler);

	// Loop client
	//while (!done) {
	// Connect to server
	conn(argv);
	//}

	return 0;
}
