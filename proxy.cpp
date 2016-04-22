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
	if (DEBUG) {
		printf("Caught signal SIGPIPE %d\n", signum);
	}
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
	if (DEBUG) {
		std::cout << "Message received from fd = " << s << std::endl;
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
	if (DEBUG) {
		std::cout << "Message sent to fd = " << s << std::endl;
	}
}

void* consume(void* data) {
	pthread_t tid = *((pthread_t*) data);

	while (!done) {
		// Consumer waits for items to process
		sem_wait(job_queue_count);

		// Process item
		int sock = -1;
		pthread_mutex_lock(count_mutex);
		if (!socketQ->empty()) {
			sock = socketQ->front();
			socketQ->pop();

//			if (DEBUG) {
//				std::cout << "Thread id = " << tid << " consumed socket = "
//						<< sock << std::endl;
//			}
		}
		pthread_mutex_unlock(count_mutex);

		// Don't allocate message buffers on stack!
		// Receive message from client
		if (!done && sock >= 0) {
			char* dataBuf = new char[MSG_BUF_SIZE];
			memset(dataBuf, '\0', MSG_BUF_SIZE);
			receiveMsg(sock, MSG_BUF_SIZE, dataBuf, true);

//			if (DEBUG) {
//				printf("Received message:\n%s\n", dataBuf);
//				fflush(stdout);
//			}

			// Parse received header information
			char* parsedCMD;
			char* parsedHost;
			char* parsedVer;
			char* parsedHeaders;

			parsedCMD = strtok(dataBuf, " ");

			// Only the GET command is supported
			if (strcmp(parsedCMD, "GET") != 0) {
				char err[] = "500 Internal Server Error";
				sendMsg(sock, strlen(err), err);
				close(sock);
				delete[] dataBuf;
				continue;
			}

			parsedHost = strtok(NULL, " ");
			parsedVer = strtok(NULL, CRLF);
			parsedHeaders = strtok(NULL, CRLF);

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
//				if (DEBUG) {
//					std::cout << "'Connection:' exists in map\nExchanging...\n"
//							<< std::endl;
//				}
				it->second = " close";
			}

//			if (DEBUG) {
//				printf("Command: %s\n", parsedCMD);
//				printf("Host: %s\n", parsedHost);
//				printf("HTTP version: %s\n", parsedVer);
//				// Print headers from map
//				for (it = headerMap.begin(); it != headerMap.end(); ++it) {
//					std::cout << it->first << it->second << std::endl;
//				}
//				fflush(stdout);
//			}

			// Now extract host and relative path
			// Must strip http://
			// Get host
			// Get port; if no port, use 80
			// Find relative path (everything after last /; are we guaranteed that a slash exists?)

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

//			if (DEBUG) {
//				std::cout << "Clean Host: " << parsedHostStr << std::endl;
//				std::cout << "Port: " << parsedPort << std::endl;
//				std::cout << "Relative Path: " << parsedRelPath << std::endl;
//			}

			/*********************************************/
			// Setup connection to webserver
			// Create struct and zero out
			struct sockaddr_in sa;
			memset(&sa, 0, sizeof(sa));

			// Check host name
			struct hostent *host = gethostbyname(parsedHostStr.c_str());
			if (!host) {
				error("Could not resolve host name");
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
//			std::cout << "My string is:" << std::endl << fullRequest
//					<< std::endl;

			char* req = new char[fullRequest.length() + 1];
			strcpy(req, fullRequest.c_str());

			// Send request
			sendMsg(web_s, strlen(req), req);
			delete[] req;

			// Receive response from webserver
			char* responseBuf = new char[MSG_BUF_SIZE];
			memset(responseBuf, '\0', MSG_BUF_SIZE);
			receiveMsg(web_s, MSG_BUF_SIZE, responseBuf, false);

//			if (DEBUG) {
//				printf("%s\n", responseBuf);
//				fflush(stdout);
//			}

			// Done with webserver
			close(web_s);

			// Send response back to client
//			if (DEBUG) {
//				std::cout << "Sending back to client (browser)" << std::endl;
//			}
			sendMsg(sock, MSG_BUF_SIZE, responseBuf);

//			if (DEBUG) {
//				std::cout << "Message sent successfully to client" << std::endl;
//			}

			// Not getting full data from Google because
			//		a. Hardcoded GET request; browser doesn't ask for missing elements
			//		b. Ending receive by checking wrong char combo (CNLFCNLF)

			// Cleanup
			delete[] dataBuf;
			delete[] responseBuf;
			if (sock >= 0) {
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
//	sem_close(job_queue_count);
//	sem_unlink(SEM_NAME);
	if (sem_destroy(job_queue_count) < 0) {
		error("Destroy semaphore failed");
	}
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

	// Signal handlers
	signal(SIGINT, termination_handler);
	signal(SIGPIPE, signal_callback_handler);

	// Synchronization
	count_mutex = new pthread_mutex_t;
	if (pthread_mutex_init(count_mutex, NULL) != 0) {
		error("mutex failed");
	}

	// Alternative to sem_init (deprecated on OSX)
	// Source:
	// http://www.linuxdevcenter.com/pub/a/linux/2007/05/24/semaphores-in-linux.html?page=5
	// Clear semaphore first
//	sem_unlink(SEM_NAME);
//	job_queue_count = sem_open(SEM_NAME, O_CREAT, SEM_PERMISSIONS, 0);
	// For testing on cs2
	job_queue_count = new sem_t;
	sem_init(job_queue_count, 0, 0);
	if (job_queue_count == SEM_FAILED) {
		//sem_unlink(SEM_NAME);
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
		} else if (DEBUG) {
			std::cout << "Spawned thread id = " << pool[i] << std::endl;
		}
	}

	// Queue for client connections
	socketQ = new std::queue<int>();

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
				std::cout << "Client (IP: " << inet_ntoa(client.sin_addr)
						<< ") connected at fd = " << client_s << std::endl;
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
