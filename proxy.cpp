/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include "p2.h"
using namespace std;

// Handles CTRL-C signals
void termination_handler(int signum) {
	printf("\nTerminating proxy...\n");
	fflush(stdout);
	done = 1;
	if (server_s > 0) {
		close(server_s);
	}
	// Fake job to wake up consumers one last time
	sem_post(job_queue_count);
}

// Send a message to a given socket file descriptor
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
	if (DEBUG) {
		printf("Message sent to fd = %d\n", s);
	}
}

void* consume(void* data) {
	pthread_t tid = *((pthread_t*) data);

	while (!done) {
		// Consumer waits for items to process
		sem_wait(job_queue_count);

		// Process item
		int sock = -1;
		pthread_mutex_lock(&count_mutex);
		if (!socketQ.empty()) {
			sock = socketQ.front();
			socketQ.pop();
			if (sock >= 0 && DEBUG) {
				cout << "Consumed socket = " << sock << endl;
			}
		}
		pthread_mutex_unlock(&count_mutex);

		// Send message to connected client
		if (sock >= 0) {
			// Send test message back to connected client via file descriptor
			// MSG_BUF_SIZE too large for stack
			char* buf = new char[MSG_BUF_SIZE];
			memset(buf, 0, MSG_BUF_SIZE);
			sprintf(buf, "This is a message for fd = %d from thread %li\n",
					sock, (unsigned long int) tid);
			sendMsg(sock, MSG_BUF_SIZE, buf);
			this_thread::sleep_for(chrono::seconds(5));
			//close(sock);
		}
	}

	pthread_exit(EXIT_SUCCESS);
	return 0;
}

void cleanup() {
	pthread_mutex_destroy(&count_mutex);
	// Alternative to sem_destroy (deprecated on OSX)
	sem_close(job_queue_count);
	sem_unlink(SEM_NAME);
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
	if (pthread_mutex_init(&count_mutex, NULL) != 0) {
		error("mutex failed");
	}

	// Alternative to sem_init (deprecated on OSX)
	// Source:
	// http://www.linuxdevcenter.com/pub/a/linux/2007/05/24/semaphores-in-linux.html?page=5
	job_queue_count = sem_open(SEM_NAME, O_CREAT, SEM_PERMISSIONS, 0);
	if (job_queue_count == SEM_FAILED) {
		sem_unlink(SEM_NAME);
		error("Unable to create semaphore");
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

	// Start consumer threads
	for (int i = 0; i < MAX_THREADS; i++) {
		if (pthread_create(&pool[i], NULL, consume, &pool[i]) < 0) {
			error("Could not create consumer thread");
		} else if (DEBUG) {
			cout << "Spawned thread id = " << pool[i] << endl;
		}
	}

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
		socketQ.push(client_s);
		pthread_mutex_unlock(&count_mutex);
		sem_post(job_queue_count);
	}

	// Wait for consumers to finish
	for (int i = 0; i < MAX_THREADS; i++) {
		if (DEBUG) {
			cout << "Joining thread " << i << endl;
		}
		pthread_join(pool[i], NULL);
	}

	// Deallocate and cleanup before exit
	cleanup();

	// Execution never gets here due to termination handler
	return 0;
}
