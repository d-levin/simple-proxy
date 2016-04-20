/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include "p2.h"
using namespace std;

// Controls termination of program
static volatile bool done = 0;

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
	while (!done) {
		// Delay for testing
		this_thread::sleep_for(chrono::seconds(5));

		//Consumer waits for items to process
		sem_wait(&job_queue_count);

		cout << "Thread awake!" << endl;

		// Process item
		pthread_mutex_lock(&count_mutex);
		if (socketQ->size() > 0) {
			cout << "Socket in queue = " << socketQ->front() << endl;
		}
		pthread_mutex_unlock(&count_mutex);

//		if (socketQ->size() > 0) {
//			int sock = socketQ->front();
//			socketQ->pop();
//			// Send test message back to connected client via file descriptor
//			char buf[MSG_BUF_SIZE];
//			sprintf(buf, "This is a message for fd = %d\n", sock);
//			sendMsg(sock, MSG_BUF_SIZE, buf);
//			close(sock);
//		}

	}

//	printf("in consume\n");
//	fflush(stdout);
//	while (!done) {
//		delay(2);
//		// Consumer waits for items to process
//		sem_wait(&job_queue_count);
//
//		// Process item
//		pthread_mutex_lock(&count_mutex);
//		if (socketQ->size() > 0) {
//			int sock = socketQ->front();
//			socketQ->pop();
//			// Send test message back to connected client via file descriptor
//			char buf[MSG_BUF_SIZE];
//			sprintf(buf, "This is a message for fd = %d\n", sock);
//			sendMsg(sock, MSG_BUF_SIZE, buf);
//			close(sock);
//		}
//		pthread_mutex_unlock(&count_mutex);
//	}

	pthread_exit(EXIT_SUCCESS);
	return 0;
}

void cleanup() {
	pthread_mutex_destroy(&count_mutex);
	sem_destroy(&job_queue_count);
	delete[] socketQ;
	delete[] pool;
}

int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 2) {
		error("usage: ./proxy <port>\n");
	}

	// Exit Handler
	// Quit on CTRL-C
	signal(SIGINT, termination_handler);
	cout << "trying" << endl;

	// Synchronization
	pthread_mutex_init(&count_mutex, NULL);
	sem_init(&job_queue_count, 0, 0);

	// Thread pool
	pool = new pthread_t[MAX_THREADS];

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

	// Start consumer threads
	for (int i = 0; i < MAX_THREADS; i++) {
		if (pthread_create(&pool[i], NULL, consume, &pool[i]) < 0) {
			error("Could not create consumer thread");
		}
	}
	cout << "threads created" << endl;

	// Queue stores client connections
	// Number of connections limited to QUEUE_SIZE
	socketQ = new queue<int> [QUEUE_SIZE];

	// Start producing
	struct sockaddr_in client;
	socklen_t csize;
	while (!done) {
		memset(&client, 0, sizeof(client));
		csize = sizeof(client);
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
	}

	// Wait for consumers to finish
	for (int i = 0; i < MAX_THREADS; i++) {
//		printf("Joining thread %d\n", i);
//		fflush(stdout);
		pthread_join(pool[i], NULL);
	}

	// Deallocate and cleanup before exit
	cleanup();

	// Execution never gets here due to termination handler
	return 0;
}
