/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include <arpa/inet.h>
#include "p2.h"
using namespace std;

// Handles CTRL-C signals
void termination_handler(int signum) {
	printf("\nTerminating program.\n");
	cleanup();
	exit(EXIT_SUCCESS);
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

void* runner(void* s) {
	// Give thread back to pool on completion
	pthread_mutex_lock(&count_mutex);
	num_threads--;
	pthread_mutex_unlock(&count_mutex);
	// Send test message back to connected client via file descriptor
	int sock = *((int*) s);
	char buf[BUF_SIZE];
	sprintf(buf, "This is a message for fd = %d from thread = %d\n", sock,
			pthread_self());
	sendMsg(sock, BUF_SIZE, buf);

	// When should socket be closed?
	//close(sock);

	// Keep thread alive for testing
	sleep(10);
	printf("Thread awake\n");
	fflush(stdout);

	// Give thread back to pool on completion
	pthread_mutex_lock(&count_mutex);
	num_threads++;
	pthread_mutex_unlock(&count_mutex);
	pthread_exit(EXIT_SUCCESS);
	return 0;
}

void* consume(void* tid) {
	while (1) {
		// Delay for a bit
		this_thread::sleep_for(chrono::seconds(2));

		if (DEBUG) {
			printf("I am consumer\n");
			printf("Number of available threads = %d\n", num_threads);
			fflush(stdout);
		}

		// Wait for producer to produce
		sem_wait(&job_queue_count);

		pthread_mutex_lock(&count_mutex);
		// Maybe change this to a while loop, else only one
		// item will get change per sem_post
		if (num_threads > 0) {
			while (!socketQ->empty()) {
				int sock = socketQ->front();
				socketQ->pop();

				// Start consuming
				pthread_t tid;
				if (pthread_create(&tid, NULL, runner, (void*) &sock) < 0) {
					error("Could not create consumer thread");
				}

				if (DEBUG) {
					printf("Job consumed!\n");
					printf("Size of job queue = %lu\n", socketQ->size());
				}
			}
		}
		pthread_mutex_unlock(&count_mutex);
	}
	return 0;
}

void cleanup() {
	// Wake up consumer thread
	pthread_kill(consumer, 0);
	pthread_mutex_destroy(&count_mutex);
	sem_destroy(&job_queue_count);
	delete[] socketQ;
	if (server_s >= 0) {
		close(server_s);
	}

}

int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 2) {
		printf("usage: ./proxy <port>\n");
		exit(EXIT_SUCCESS);
	}

	// Exit Handler
	// Quit on CTRL-C
	signal(SIGINT, termination_handler);

	// Initialize variables for synchronization and multi-threading
	pthread_mutex_init(&count_mutex, NULL);
	sem_init(&job_queue_count, 0, 0);
	num_threads = MAX_THREADS;

	// Set up connection and start sending/receiving
	// Create structs and zero out
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	// Set struct properties
	int port = atoi(argv[SERVER_PORT]);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	// Open socket
	server_s = socket(AF_INET, SOCK_STREAM, 0);

	// Socket failed
	if (server_s < 0)
		error("Failed to open socket");

	// Bind
	if (bind(server_s, (struct sockaddr*) &server, sizeof(server)) < 0)
		error("Failed to bind");

	// Wait for connections
	// Accepting up to MAX_CONN pending connections
	printf("Listening on port %d...\n", port);
	if (listen(server_s, MAX_CONN) < 0) {
		error("Failed to listen");
	}

	// Start consuming
	if (pthread_create(&consumer, NULL, consume, consumer) < 0) {
		error("Could not create consumer thread");
	}

	// Queue stores client connections
	// Number of connections limited to MAXCONN
	socketQ = new queue<int> [QUEUE_SIZE];

	// Start producing
	struct sockaddr_in client;
	memset(&client, 0, sizeof(client));
	socklen_t csize = sizeof(client);
	while (1) {
		// Accept client connection
		int client_s = accept(server_s, (struct sockaddr*) &client, &csize);

		if (client_s < 0) {
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

		/***** For debugging *****/
		if (DEBUG) {
			printf("Client IP-address = %s\n", inet_ntoa(client.sin_addr));
			printf("Elements in queue = %lu\n", socketQ->size());
			fflush(stdout);
		}
		/*************************/
	}

	// Execution never gets here due to termination handler
	return 0;
}
