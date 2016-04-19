/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include "p2.h"
using namespace std;

#include <thread>
#include <chrono>

// Receive a message from given connection
void getCommand(int client_s, int size, char *ptr) {
	int received = 0;
	while ((size > 0) && (received = read(client_s, ptr, size))) {
		// Check error
		if (received < 0) {
			error("Read failed");
		}

		// Move pointer forward to next piece of data
		ptr += received;

		// The command has been fully received
//		if ((*ptr) == '\0') {
//			printf("Command received\n");
//			fflush(stdout);
//			// No need to continue loop
//			break;
//		}

// Keep track of how much data has been sent
// Prevents the buffer from overflowing
		size -= received;
	}
}

// Handles CTRL-C signals
void termination_handler(int signum) {
	printf("\nTerminating program.\n");
	done = 1;
	exit(EXIT_SUCCESS);
}

// Thread function
void *runner(void *vptr_client_s) {
	// Decrement num_threads to limit number of concurrent threads
	pthread_mutex_lock(&count_mutex);
	num_threads--;
	pthread_mutex_unlock(&count_mutex);

	// Self-identify if DEBUG on
	if (DEBUG) {
		pthread_t ptid = pthread_self();
		uint64_t tid = 0;
		memcpy(&tid, &ptid, min(sizeof(tid), sizeof(ptid)));
		cout << "Thread ID = " << tid << endl;
	}

	// Cast back to int
	int client_s = *((int*) vptr_client_s);

	// Receive command from client
	// Browser will send GET requests over the socket
	char buf[BUF_SIZE];
	getCommand(client_s, BUF_SIZE, buf);
	printf("input = %s", buf);

	// Parse the command
	char *cmd = strtok(buf, " ");
	char *host = strtok(NULL, " ");
	char *protocol = strtok(NULL, " ");
	printf("cmd = %s\n", cmd);
	printf("host = %s\n", host);
	printf("protocol = %s\n", protocol);
	fflush(stdout);

//
//	// Send response
//	ptr = (char*) &m;
//	int sent = 0;
//	size = sizeof(m);
//	sendMsg(client_s, size, sent, ptr);

	// Close here after work is done
	close(client_s);

	// Make space for new thread
	pthread_mutex_lock(&count_mutex);
	num_threads++;
	pthread_mutex_unlock(&count_mutex);

	pthread_exit(EXIT_SUCCESS);

	return 0;
}

void* consume(void* unused) {
	while (!done) {
		// Wait for producer to produce
		sem_wait(&job_queue_count);

		// Delay for a bit
		std::this_thread::sleep_for(std::chrono::seconds(2));

		// Make sure there are available threads to process request
		pthread_mutex_lock(&count_mutex);
		if (num_threads > 0) {
			printf("Consuming...\n");
			printf("Queue size = %d", socketQ->empty());
			fflush(stdout);
			if (!socketQ->empty()) {
				int client_s = socketQ->front();
				printf("Popped from queue = %d\n", client_s);
				fflush(stdout);
				socketQ->pop();
				close(client_s);
			}
			//pthread_t tid;
//			if (pthread_create(&tid, NULL, runner, (void*) &client_s) < 0) {
//				error("Could not create thread");
//			}
			// decrement num_threads here. Does execution get here if thread create is successful?

			// Don't wait for threads to finish
			//pthread_detach (tid);
		} else {
			printf("No available threads");
			fflush(stdout);
		}
		pthread_mutex_unlock(&count_mutex);

	}

	return 0;
}

void* produce(void* s) {
	// Loop program
	// Connection handled by new socket
	struct sockaddr_in client;
	memset(&client, 0, sizeof(client));
	int sock = *((int*) s);
	int client_s;
	socklen_t csize = sizeof(client);
	while (!done
			&& (client_s = accept(sock, (struct sockaddr*) &client, &csize))) {
		if (client_s < 0) {
			error("Failed to accept");
		}
		printf("I am producer\n");
		printf("socket id = %d\n", client_s);
		fflush(stdout);

//		// Process new sockets
//		pthread_mutex_lock(&count_mutex);
//		// Add to socket queue if queue is not full
//		// Assume proxy will accept a maximum of QUEUE_SIZE connections
//		if (socketQ->size() < QUEUE_SIZE) {
//			socketQ->push(client_s);
//			// Signal consumer
//			sem_post(&job_queue_count);
//		} else {
//			error("Proxy has reached maximum connections. Exiting.");
//		}
//		pthread_mutex_unlock(&count_mutex);
	}
	return 0;
}

// Open connection
// Spawn threads on connect
void connect(char *argv[]) {
	// Create structs and zero out
	struct sockaddr_in server;
	struct sockaddr_in client;
	memset(&server, 0, sizeof(server));
	memset(&client, 0, sizeof(client));

	// Set struct properties
	int port = atoi(argv[SERVER_PORT]);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	// Open socket
	int s = socket(AF_INET, SOCK_STREAM, 0);

	// Socket failed
	if (s < 0)
		error("Failed to open socket");

	// Bind
	if (bind(s, (struct sockaddr*) &server, sizeof(server)) < 0)
		error("Failed to bind");

	// Wait for connections
	printf("Listening on port %d...\n", port);
	if (listen(s, MAXCONN) < 0) {
		error("Failed to listen");
	}

	// Support multithreading
	pthread_t producer;
	pthread_t consumer;

	// Spawn producer/consumer threads
	pthread_create(&consumer, NULL, consume, NULL);
	pthread_create(&producer, NULL, produce, &s);

	// Join threads before exiting
	pthread_join(consumer, NULL);
	pthread_join(producer, NULL);

//	// Loop program
//	// Connection handled by new socket
//	int client_s;
//	socklen_t csize = sizeof(client);
//	while (!done && (client_s = accept(s, (struct sockaddr*) &client, &csize))) {
//		if (client_s < 0) {
//			error("Failed to accept");
//		}
//
//		// Process new sockets
//		pthread_mutex_lock(&count_mutex);
//		// Add to socket queue if queue is not full
//		// Assume proxy will accept a maximum of QUEUE_SIZE connections
//		if (socketQ->size() < QUEUE_SIZE) {
//			socketQ->push(client_s);
//			// Signal consumer
//			sem_post(&job_queue_count);
//		} else {
//			error("Proxy has reached maximum connections. Exiting.");
//		}
//		pthread_mutex_unlock(&count_mutex);
//	}
}

void cleanup() {
	pthread_mutex_destroy(&count_mutex);
	sem_destroy(&job_queue_count);
	delete[] socketQ;
}

int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 2) {
		printf("usage: ./proxy <port>\n");
		exit(EXIT_SUCCESS);
	}

	// Handler
	// Quit on CTRL-C
	signal(SIGINT, termination_handler);

	// Initialize variables
	pthread_mutex_init(&count_mutex, NULL);
	num_threads = MAX_THREADS;
	socketQ = new queue<int> [QUEUE_SIZE];
	sem_init(&job_queue_count, 0, 0);

	// Set up connection and start sending/receiving
	connect(argv);

	// Deallocation
	cleanup();

	return 0;
}
