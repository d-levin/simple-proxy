/*
 *    Filename: proxy.cpp
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 * Description: Implementation of a simple web proxy.
 * 			  	Works only with http (not https) and HTTP/1.0
 * 			  	Any 'connection' header sent to the destination web
 * 			  	server will be replaced with 'connection: close'
 * 			  	to emulate HTTP/1.0
 */

#include "proxy.h"

// Handles CTRL-C signals
void termination_handler(int signum) {
	std::cout << std::endl << "Terminating proxy..." << std::endl;
	fflush(stdout);
	done = 1;
	shutdown(server_s, SHUT_RDWR);
	close(server_s);
	// Fake job to wake up consumers one last time
	for (int i = 0; i < MAX_THREADS; i++) {
		sem_post(job_queue_count);
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
}

// Send a message to a given socket file descriptor
void sendMsg(int s, int size, char *ptr) {
	int sent = 0;
	while ((size > 0) && (sent = write(s, ptr, size))) {
		// Check error
		if (sent < 0) {
			break;
			//error("Write failed");
		}

		// Move pointer forward in struct by size of sent data
		ptr += sent;

		// Keep track of how much data has been sent
		size -= sent;
	}
}

void cleanOnError(int sock, char* &dataBuf) {
	if (sock > -1) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
	}
	delete[] dataBuf;
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

			// Parse received header information
			char* savePtr;
			char* parsedCMD;
			char* parsedHost;
			char* parsedVer;
			char* parsedHeaders;

			parsedCMD = strtok_r(dataBuf, " ", &savePtr);
			parsedHost = strtok_r(NULL, " ", &savePtr);
			parsedVer = strtok_r(NULL, CRLF, &savePtr);
			parsedHeaders = strtok_r(NULL, CRLF, &savePtr);

			// Verify information and command
			if (!parsedCMD || (strcmp(parsedCMD, "GET") != 0) || !parsedHost
					|| !parsedVer) {
				sendMsg(sock, strlen(errMsg), (char*) errMsg);
				cleanOnError(sock, dataBuf);
				continue;
			}

			// Replace HTTP version
			if (strcmp(parsedVer, "HTTP/1.1") == 0) {
				parsedVer = (char*) "HTTP/1.0";
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
					// Convert header field to lowercase before inserting
					std::string key = std::string(parsedHeaders).substr(0,
							index + 1);
					std::transform(key.begin(), key.end(), key.begin(),
							::tolower);
					std::string value = std::string(parsedHeaders).substr(
							index + 1);
					tempPair = std::make_pair(key, value);
					headerMap.insert(tempPair);
				}
				// Each header is on a new line
				parsedHeaders = strtok_r(NULL, CRLF, &savePtr);
			}

			// Replace HTTP version?

			// Replace headers
			std::map<std::string, std::string>::iterator it = headerMap.find(
					"connection:");
			if (it != headerMap.end()) {
				it->second = " close";
			} else {
				// Add connection header if missing
				std::pair<std::string, std::string> tempPair;
				tempPair = std::make_pair("connection:", " close");
				headerMap.insert(tempPair);
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

			// Built request string
			std::stringstream ss;
			ss << parsedCMD << " " << parsedRelPath << " " << parsedVer << CRLF;

			// Add all headers
			for (it = headerMap.begin(); it != headerMap.end(); ++it) {
				ss << it->first << it->second << CRLF;
			}

			// If host not included, add
			it = headerMap.find("host:");
			if (it == headerMap.end()) {
				ss << "Host: " << parsedHostStr << CRLF;
			}

			// End with last CRLF to mark end of header
			ss << CRLF;
			std::string fullRequest = ss.str();

			if (DEBUG) {
				std::cout << fullRequest << std::endl;
			}

			// Cleanup stream
			ss.str(std::string());
			ss.clear();

			/* Setup connection to webserver */
			struct sockaddr_in sa;
			bzero((char*) &sa, sizeof(sa));

			// Verify host name
			struct hostent *host = gethostbyname(parsedHostStr.c_str());
			if (!host) {
				cleanOnError(sock, dataBuf);
				continue;
			}

			// Copy host name to struct
			memcpy(&sa.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

			// Set struct properties
			int port = atoi(parsedPort.c_str());
			sa.sin_family = AF_INET;
			sa.sin_port = htons(port);

			// Open socket to webserver
			int web_s;
			if ((web_s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				cleanOnError(sock, dataBuf);
				continue;
			}

			// Connect to host
			if (connect(web_s, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
				cleanOnError(sock, dataBuf);
				continue;
			}

			// Prepare request for transmission
			char* req = new char[fullRequest.length() + 1];
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
			shutdown(web_s, SHUT_RDWR);
			close(web_s);

			// Send response back to client
			sendMsg(sock, MSG_BUF_SIZE, responseBuf);

			// Cleanup
			delete[] dataBuf;
			delete[] responseBuf;
			shutdown(sock, SHUT_RDWR);
			close(sock);
		}
	}

	pthread_exit(EXIT_SUCCESS);
	return 0;
}

void cleanup() {
	// Wait for consumers to finish
	for (int i = 0; i < MAX_THREADS; i++) {
		pthread_join(pool[i], NULL);
	}

	pthread_mutex_destroy(count_mutex);
	// Alternative to sem_destroy (deprecated on OSX)
	//sem_close(job_queue_count);
	//sem_unlink(SEM_NAME);
	if (sem_destroy(job_queue_count) < 0) {
		error("Destroy semaphore failed");
	}

	// First empty queue then deallocate
	while (!socketQ->empty()) {
		socketQ->pop();
	}
	delete socketQ;
	delete[] pool;
}

void setupSigHandlers() {
	// Handle CTRL-C signals
	signal(SIGINT, termination_handler);

	// Handle SIGPIPE signals
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);
}

void initSynchronization() {
	count_mutex = new pthread_mutex_t;
	if (pthread_mutex_init(count_mutex, NULL) != 0) {
		error("Mutex init failed");
	}

	// Alternative to sem_init (deprecated on OSX)
	// Clear semaphore first
//	sem_unlink(SEM_NAME);
//	job_queue_count = sem_open(SEM_NAME, O_CREAT, SEM_PERMISSIONS, 0);
	// For running on cs2 to avoid permission errors
	job_queue_count = new sem_t;
	sem_init(job_queue_count, 0, 0);
	if (job_queue_count == SEM_FAILED) {
		//sem_unlink(SEM_NAME);
		error("Unable to create semaphore");
	}
}

void initThreadPool() {
	// Thread pool
	pool = new pthread_t[MAX_THREADS];

	// Start consumer threads
	for (int i = 0; i < MAX_THREADS; i++) {
		if (pthread_create(&pool[i], NULL, consume, &pool[i]) < 0) {
			error("Could not create consumer thread");
		}
	}
}

int main(int argc, char *argv[]) {
	// Check number of arguments
	if (argc < 2) {
		error("usage: ./proxy <port>\n");
	}

	// Catch CTRL-C and SIGPIPE
	setupSigHandlers();

	// Setup thread synchronization
	initSynchronization();

	// Start the thread pool
	initThreadPool();

	/* Set up server to listen for incoming connections */
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	// Set server struct properties
	int port = atoi(argv[LISTEN_PORT]);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	// Open socket
	if ((server_s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		error("Failed to open socket");
	}

	// Assign address to socket
	if (bind(server_s, (struct sockaddr*) &server, sizeof(server)) < 0) {
		error("Failed to bind");
	}

	// Wait for connections
	std::cout << "Listening on port " << port << "..." << std::endl;
	if (listen(server_s, SOMAXCONN) < 0) {
		error("Failed to listen");
	}

	/* Start producing */
	// Queue for client connections
	socketQ = new std::queue<int>();
	struct sockaddr_in client;
	memset(&client, 0, sizeof(client));
	socklen_t csize = sizeof(client);
	while (!done) {
		// Accept client connection
		int client_s = accept(server_s, (struct sockaddr*) &client, &csize);

		// Prevent error from showing if program is exiting
		// Not a reason to terminate proxy
		if (client_s < 0 && !done) {
			shutdown(client_s, SHUT_RDWR);
			close(client_s);
			continue;
		}

		// Add to queue
		pthread_mutex_lock(count_mutex);
		// The number of sockets in the queue are limited to QUEUE_SIZE
		socketQ->push(client_s);
		pthread_mutex_unlock(count_mutex);
		// Ready for consumption
		sem_post(job_queue_count);
	}

	// Deallocate and cleanup before exit
	cleanup();

	// Execution never gets here due to termination handler
	return 0;
}
