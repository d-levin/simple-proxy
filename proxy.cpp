/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include <arpa/inet.h>
#include "p2.h"
using namespace std;

#ifdef DEBUG
void delay(unsigned int sec) {
	this_thread::sleep_for(chrono::seconds(sec));
}
#endif

// Handles CTRL-C signals
void termination_handler(int signum) {
	printf("\nTerminating program.\n");
	fflush(stdout);
	done = 1;
	if (server_s > 0) {
		close(server_s);
	}
}

// Send a message to given connection
void sendMsg(int s, int size, char *ptr) {
	int sent = 0;
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

//void* runner(void* s) {
//	// Send test message back to connected client via file descriptor
//	int sock = *((int*) s);
//	char buf[MSG_BUF_SIZE];
//	sprintf(buf, "This is a message for fd = %d from thread = %d\n", sock,
//			pthread_self());
//	sendMsg(sock, MSG_BUF_SIZE, buf);
//
//	// When should socket be closed?
//	//close(sock);
//
//	// Keep thread alive for testing
//	sleep(10);
//	printf("Thread awake\n");
//	fflush(stdout);
//
//	pthread_exit(EXIT_SUCCESS);
//	return 0;
//}

void* consume(void* t) {
#ifdef DEBUG
	int tid = *((int*) t);
	printf("I am thread = %d\n", tid);
	fflush(stdout);
#endif

	while (!done) {
		delay(2);
		printf("I am thread\n");
		fflush(stdout);
//		// Consumer waits for items to process
//		sem_wait(&job_queue_count);
//
//		pthread_mutex_lock(&count_mutex);
//		if (socketQ->size() > 0) {
//			int sock = socketQ->front();
//			socketQ->pop();
//			close(sock);
//#ifdef DEBUG
//			printf("Thread id %lu consumed socket %d\n", tid, sock);
//#endif
//		}
//		pthread_mutex_unlock(&count_mutex);
	}

	pthread_exit(EXIT_SUCCESS);
	return 0;
}

void cleanup() {
	pthread_mutex_destroy(&count_mutex);
	sem_destroy(&job_queue_count);
	delete[] socketQ;
}

int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 2) {
		error("usage: ./proxy <port>\n");
	}

	// Exit Handler
	// Quit on CTRL-C
	signal(SIGINT, termination_handler);

	// Synchronization
	pthread_mutex_init(&count_mutex, NULL);
	sem_init(&job_queue_count, 0, 0);

	// Thread pool
	pthread_t pool[MAX_THREADS];

	// Start consumer threads
	for (int i = 0; i < MAX_THREADS; i++) {
#ifdef DEBUG
		printf("Creating thread %d\n", i);
		fflush(stdout);
#endif
		if (pthread_create(&pool[i], NULL, consume, NULL) < 0) {
			error("Could not create consumer thread");
		}
	}

	// Set up server to listen for incoming connections
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	// Set server struct properties
	int port = atoi(argv[LISTEN_PORT]);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	// Open socket
	server_s = socket(AF_INET, SOCK_STREAM, 0);

	// Socket failed
	if (server_s < 0) {
		error("Failed to open socket");
	}

	// Bind
	if (bind(server_s, (struct sockaddr*) &server, sizeof(server)) < 0) {
		error("Failed to bind");
	}

	// Wait for connections
	// Accepting up to MAX_CONN pending connections
	printf("Listening on port %d...\n", port);
	if (listen(server_s, MAX_CONN) < 0) {
		error("Failed to listen");
	}

	// Queue stores client connections
	// Number of connections limited to QUEUE_SIZE
	socketQ = new queue<int> [QUEUE_SIZE];

	// Start producing
	struct sockaddr_in client;
	memset(&client, 0, sizeof(client));
	socklen_t csize = sizeof(client);
	while (!done) {
		// Accept client connection
		int client_s = accept(server_s, (struct sockaddr*) &client, &csize);

		// Prevent error from showing if program is exiting
		if (client_s < 0 && !done) {
			error("Failed to accept");
		}

		// Add to queue
		pthread_mutex_lock(&count_mutex);
		// The number of sockets in the queue are limited to QUEUE_SIZE
		if (socketQ->size() < QUEUE_SIZE) {
			socketQ->push(client_s);
			sem_post(&job_queue_count);
		} else {
			error("Socket queue full");
		}
		pthread_mutex_unlock(&count_mutex);

#ifdef DEBUG
		printf("Client IP-address = %s\n", inet_ntoa(client.sin_addr));
		printf("Elements in queue = %lu\n", socketQ->size());
		fflush(stdout);
#endif

	}

	// Wait for consumers to finish
	for (int i = 0; i < MAX_THREADS; i++) {
		printf("Joining thread %d\n", i);
		fflush(stdout);
		pthread_join(pool[i], NULL);
	}

	// Deallocate and cleanup before exit
	cleanup();

	// Execution never gets here due to termination handler
	return 0;
}
