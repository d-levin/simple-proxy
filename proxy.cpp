/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include "p2.h"
using namespace std;

// Handles CTRL-C signals
void termination_handler(int signum) {
	cout << endl << "Terminating proxy..." << endl;
	fflush(stdout);
	done = 1;
	close(server_s);
	// Fake job to wake up consumers one last time
	for (int i = 0; i < MAX_THREADS; i++) {
		sem_post(job_queue_count);
	}
}

// Receive a message to a given socket file descriptor
void receiveMsg(int s, int size, char *ptr) {
	int received = 0;
	// First time we enter the ptr points to start of buffer
	char* buf = ptr;
	// Subtract 1 to avoid completely filling buffer
	// Leave space for '\0'
	while ((size > 0) && (received = read(s, ptr, size - 1))) {
		// Check error
		if (received < 0 && !done) {
			error("Read failed");
		}

		if (received == 0) {
			cout << "Client closed connection" << endl;
		}

		// Move pointer forward in struct by size of received data
		ptr += received;

		// Keep track of how much data has been received
		size -= received;

		// Exit loop if CRLF CRLF has been received
		if (strstr(buf, CRLF CRLF)) {
			break;
		}
	}
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
		cout << "Message sent to fd = " << s << endl;
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

			if (DEBUG) {
				cout << "Thread id = " << tid << " consumed socket = " << sock
						<< endl;
			}
		}
		pthread_mutex_unlock(&count_mutex);

		// Don't allocate buffer on stack
		// Receive message from client
		if (!done) {
			char* buf = new char[MSG_BUF_SIZE];
			memset(buf, 0, MSG_BUF_SIZE);
			receiveMsg(sock, MSG_BUF_SIZE, buf);
			printf("Received message = %s\n", buf);
			fflush(stdout);

			/*********************************************/
			// Setup connection to webserver
			// Create struct and zero out
			struct sockaddr_in sa;
			memset(&sa, 0, sizeof(sa));

			// Check host name
			char hname[] = "http://www.yahoo.com";
			struct hostent *host = gethostbyname(hname);
			if (!host) {
				error("Could not resolve host name");
			}

			// Copy host name to struct
			memcpy(&sa.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

			// Set struct properties
			int port = atoi("80");
			sa.sin_family = AF_INET;
			sa.sin_port = htons(port);

			// Open socket
			int web_s = socket(AF_INET, SOCK_STREAM, 0);

			// Socket failed
			if (web_s < 0) {
				error("Failed to open socket");
			}

			// Connect to host
			if (connect(web_s, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
				error("Connection failed");
			} else {
				cout << "Connected to " << hname << endl;
			}
			/*********************************************/

			// Cleanup
			delete[] buf;
			close(sock);
		}

//		// Send test message back to connected client via file descriptor
//		memset(buf, 0, MSG_BUF_SIZE);
//		sprintf(buf, "This is a message for fd = %d from thread %li\n", sock,
//				(unsigned long int) tid);
//		sendMsg(sock, MSG_BUF_SIZE, buf);

		//this_thread::sleep_for(chrono::seconds(5));

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
	// Clear semaphore first
	sem_unlink(SEM_NAME);
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
	cout << "Listening on port " << port << "..." << endl;
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

		if (DEBUG) {
			if (!done) {
				cout << "Client (IP: " << inet_ntoa(client.sin_addr)
						<< ") connected at fd = " << client_s << endl;
			}
		}

		// Add to queue
		pthread_mutex_lock(&count_mutex);
		// The number of sockets in the queue are limited to QUEUE_SIZE
		socketQ.push(client_s);
		pthread_mutex_unlock(&count_mutex);
		// Ready for consumption
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
