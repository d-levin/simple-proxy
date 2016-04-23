/*
 *    Filename: proxy.h
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 * Description: Prototype for simple proxy server
 */

#ifndef PROXY_H_
#define PROXY_H_

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <unistd.h>
#include <string.h>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <queue>
#include <semaphore.h>
#include <arpa/inet.h>
#include <map>

#define LISTEN_PORT		1
#define MAX_THREADS		30
#define CRLF "\r\n"
// Permissions are READ/WRITE for OWNER and READ for others
// Only for use on systems where sem_init/sem_destroy are deprecated (OSX)
#define SEM_PERMISSIONS	0644

// Send/receive message buf size 1MB
#define MSG_BUF_SIZE	1000000

// Set to 0 to disable debug messages
#define DEBUG			0

// Multithreading and thread safety
pthread_mutex_t* count_mutex;
sem_t* job_queue_count;
char SEM_NAME[] = "sem";
int server_s;
std::queue<int>* socketQ;
pthread_t* pool;

// Error message for client
const char* errMsg = "500 Internal Error\r\n\r\n";

// Controls termination of program
static volatile bool done = 0;

// Custom error handling
void error(const char *err) {
	perror(err);
	exit(EXIT_FAILURE);
}

// Function definitions
void termination_handler(int signum);
void receiveMsg(int s, int size, char *ptr, bool headerOnly);
void sendMsg(int s, int size, char *ptr);
void* consume(void* data);
void cleanup();
void setupSigHandlers();
void initSynchronization();

#endif /* PROXY_H_ */
