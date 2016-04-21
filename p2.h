/*
 *    Filename: p2.h
 *  Created on: Apr 11, 2016
 *      Author: Dennis Levin
 * Description: Prototype for Project 2
 */

#ifndef P2_H_
#define P2_H_

#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <unistd.h>
#include <string.h>
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

// For built in thread delay
//#include <thread>
//#include <chrono>

#define LISTEN_PORT		1
#define MAX_CONN		5
#define MAX_THREADS		1 // Set to 30
#define CRLF "\r\n"
// Permissions are READ/WRITE for OWNER and READ for others
#define SEM_PERMISSIONS	0644

// Array sizes
#define MSG_BUF_SIZE	1000000 // Set to 1MB = 1000000 bytes

// Set to 0 to disable debug messages
#define DEBUG			1

// Multithreading and thread safety
pthread_mutex_t* count_mutex;
sem_t* job_queue_count;
char SEM_NAME[] = "sem";
int server_s;
std::queue<int>* socketQ;
pthread_t* pool;

// Controls termination of program
static volatile bool done = 0;

// Custom error handling
void error(const char *err) {
	perror(err);
	exit(EXIT_FAILURE);
}

// Function definitions
void cleanup();

#endif /* P2_H_ */
