/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 */

#include "p2.h"
//using namespace std;

// Handles CTRL-C signals
void termination_handler(int signum) {
	std::cout << std::endl << "Terminating proxy..." << std::endl;
	fflush(stdout);
	done = 1;
	close(server_s);
	// Fake job to wake up consumers one last time
	for (int i = 0; i < MAX_THREADS; i++) {
		sem_post(job_queue_count);
	}
}

// Catch SIGPIPE error
void signal_callback_handler(int signum) {
	printf("Caught signal SIGPIPE %d\n", signum);
}

// Receive a message to a given socket file descriptor
void receiveMsg(int s, int size, char *ptr, bool headerOnly) {
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

		// Move pointer forward in struct by size of received data
		ptr += received;

		// Keep track of how much data has been received
		size -= received;

		// Read only header
		if (headerOnly) {
			// Exit loop if CRLF CRLF has been received
			if (strstr(buf, CRLF CRLF)) {
				break;
			}
		}
	}
}

// Send a message to a given socket file descriptor
void sendMsg(int s, int size, char *ptr) {
	int sent = 0;
	while ((size > 0) && (sent = write(s, ptr, size))) {
		// Check error
		if (sent < 0) {
			break;
			error("Write failed");
		}

		// Move pointer forward in struct by size of sent data
		ptr += sent;

		// Keep track of how much data has been sent
		size -= sent;
	}
}

void* consume(void* data) {
	while (!done) {
		// Consumer waits for items to process
		sem_wait(job_queue_count);

		// Process item
		int sock = -1;
		pthread_mutex_lock(count_mutex);
		if (!socketQ->empty()) {
			sock = socketQ->front();
			socketQ->pop();
		}
		pthread_mutex_unlock(count_mutex);

		// Don't allocate message buffers on stack!
		// Receive message from client
		if (!done && sock >= 0) {
			char* dataBuf = new char[MSG_BUF_SIZE];
			memset(dataBuf, '\0', MSG_BUF_SIZE);
			receiveMsg(sock, MSG_BUF_SIZE, dataBuf, true);

			// Change parsing here to only use std::strings
			// to avoid memory leaks?

			// When to close sockets? After read/write?

			// Parse received header information
			char* parsedCMD;
			char* parsedHost;
			char* parsedVer;
			char* parsedHeaders;

			parsedCMD = strtok(dataBuf, " ");

			// Only the GET command is supported
			if (!parsedCMD || strcmp(parsedCMD, "GET") != 0) {
				char err[] = "500 Internal Server Error";
				sendMsg(sock, strlen(err), err);
				close(sock);
				delete[] dataBuf;
				continue;
			}

			parsedHost = strtok(NULL, " ");
			parsedVer = strtok(NULL, CRLF);
			parsedHeaders = strtok(NULL, CRLF);

			if (!parsedHost || !parsedVer) {
				char err[] = "500 Internal Server Error";
				sendMsg(sock, strlen(err), err);
				close(sock);
				delete[] dataBuf;
				continue;
			}

			// Parse the headers into map as <key, value>
			std::string::size_type index;
			std::map<std::string, std::string> headerMap;
			while (parsedHeaders != NULL) {
				// Now split by :
				index = std::string(parsedHeaders).find(':', 0);
				if (index != std::string::npos) {
					std::pair<std::string, std::string> tempPair;
					// Include the :
					tempPair = std::make_pair(
							std::string(parsedHeaders).substr(0, index + 1),
							std::string(parsedHeaders).substr(index + 1));
					headerMap.insert(tempPair);
				}
				// Each header is on a new line
				// IS THIS OK OR WILL THE FUNCTION DELIMIT ON ONLY CR OR LF ALSO?
				parsedHeaders = strtok(NULL, CRLF);
			}

			// Replace headers
			std::map<std::string, std::string>::iterator it = headerMap.find(
					"Connection:");
			if (it != headerMap.end()) {
				it->second = " close";
			}

			// Remove http://
			std::string parsedHostStr = std::string(parsedHost);
			std::string http = "http://";
			index = parsedHostStr.find(http);
			if (index != std::string::npos) {
				parsedHostStr.erase(index, http.length());
			}

			// Get relative path
			std::string parsedRelPath;
			index = parsedHostStr.find('/');
			if (index != std::string::npos) {
				parsedRelPath = parsedHostStr.substr(index);
				// Delete relative path from host str
				parsedHostStr.erase(index, parsedRelPath.length());
			}

			// Find port
			std::string parsedPort;
			index = parsedHostStr.find(':');
			if (index != std::string::npos) {
				parsedPort = parsedHostStr.substr(index + 1);
			} else {
				parsedPort = "80";
			}

			/*********************************************/
			// Setup connection to webserver
			// Create struct and zero out
			struct sockaddr_in sa;
			bzero((char*) &sa, sizeof(sa));

			// Check host name
			struct hostent *host = gethostbyname(parsedHostStr.c_str());
			if (!host) {
				//error("Could not resolve host name");
				delete[] dataBuf;
				continue;
			}

			// Copy host name to struct
			memcpy(&sa.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

			// Set struct properties
			int port = atoi(parsedPort.c_str());
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
			}

			// Built request string
			std::stringstream ss;
			ss << parsedCMD << " " << parsedRelPath << " " << parsedVer << CRLF
					<< "Host: " << parsedHostStr << CRLF;
			// Add all headers
			for (it = headerMap.begin(); it != headerMap.end(); ++it) {
				ss << it->first << it->second << CRLF;
			}
			// End with last CRLF to mark end of header
			ss << CRLF;
			std::string fullRequest = ss.str();
			ss.str(std::string());
			ss.clear();

			char* req = new char[fullRequest.length() + 1];
			// Tried 0 and '\0' here, didn't matter, still seg faults
			memset(req, '\0', fullRequest.length() + 1);
			strcpy(req, fullRequest.c_str());

			// Send request
			sendMsg(web_s, strlen(req), req);
			delete[] req;

			// Receive response from webserver
			char* responseBuf = new char[MSG_BUF_SIZE];
			memset(responseBuf, '\0', MSG_BUF_SIZE);
			receiveMsg(web_s, MSG_BUF_SIZE, responseBuf, false);

			// Done with webserver
			close(web_s);

			// Send response back to client
			sendMsg(sock, MSG_BUF_SIZE, responseBuf);

			// Cleanup
			delete[] dataBuf;
			delete[] responseBuf;
			if (sock >= 0) {
				std::cout << "Socket " << sock << " was closed!" << std::endl;
				close(sock);
			}
		}
	}

	pthread_exit(EXIT_SUCCESS);
	return 0;
}

void cleanup() {
	pthread_mutex_destroy(count_mutex);
	// Alternative to sem_destroy (deprecated on OSX)
	sem_close(job_queue_count);
	sem_unlink(SEM_NAME);
//	if (sem_destroy(job_queue_count) < 0) {
//		error("Destroy semaphore failed");
//	}
	while (!socketQ->empty()) {
		socketQ->pop();
	}
	delete socketQ;
	delete[] pool;
}

int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 2) {
		error("usage: ./proxy <port>\n");
	}

	// Handle CTRL-C
	signal(SIGINT, termination_handler);

	// Handle SIGPIPE
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);

	// Synchronization
	count_mutex = new pthread_mutex_t;
	if (pthread_mutex_init(count_mutex, NULL) != 0) {
		error("mutex failed");
	}

	// Alternative to sem_init (deprecated on OSX)
	// Clear semaphore first
	sem_unlink(SEM_NAME);
	job_queue_count = sem_open(SEM_NAME, O_CREAT, SEM_PERMISSIONS, 0);
	// For running on cs2 to avoid permission errors
//	job_queue_count = new sem_t;
//	sem_init(job_queue_count, 0, 0);
	if (job_queue_count == SEM_FAILED) {
		//sem_unlink(SEM_NAME);
		error("Unable to create semaphore");
	}

	// Set up server to listen for incoming connections
	struct sockaddr_in server;
	bzero((char*) &server, sizeof(server));

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

	// Assign address to socket
	if (bind(server_s, (struct sockaddr*) &server, sizeof(server)) < 0) {
		error("Failed to bind");
	}

	// Wait for connections
	// Accepting up to MAX_CONN pending connections
	std::cout << "Listening on port " << port << "..." << std::endl;
	if (listen(server_s, MAX_CONN) < 0) {
		error("Failed to listen");
	}

	// Thread pool
	pool = new pthread_t[MAX_THREADS];

	// Start consumer threads
	for (int i = 0; i < MAX_THREADS; i++) {
		if (pthread_create(&pool[i], NULL, consume, &pool[i]) < 0) {
			error("Could not create consumer thread");
		}
	}

	// Queue for client connections
	socketQ = new std::queue<int>();

	// Start producing
	struct sockaddr_in client;
	bzero((char*) &server, sizeof(server));
	socklen_t csize = sizeof(client);
	while (!done) {
		// Accept client connection
		int client_s = accept(server_s, (struct sockaddr*) &client, &csize);

		// Prevent error from showing if program is exiting
		if (client_s < 0 && !done) {
			continue;
			//error("Failed to accept");
		}

		if (DEBUG) {
			if (!done) {
				std::cout << "Socket " << client_s << " was opened!"
						<< std::endl;
			}
		}

		// Add to queue
		pthread_mutex_lock(count_mutex);
		// The number of sockets in the queue are limited to QUEUE_SIZE
		socketQ->push(client_s);
		pthread_mutex_unlock(count_mutex);
		// Ready for consumption
		sem_post(job_queue_count);
	}

	// Wait for consumers to finish
	for (int i = 0; i < MAX_THREADS; i++) {
		if (DEBUG) {
			std::cout << "Joining thread " << i << std::endl;
		}
		pthread_join(pool[i], NULL);
	}

	// Deallocate and cleanup before exit
	cleanup();

	// Execution never gets here due to termination handler
	return 0;
}
