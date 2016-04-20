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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <iostream>
#include <queue>
#include <semaphore.h>
#include <thread>
#include <chrono>
#include <arpa/inet.h>

#define LISTEN_PORT		1
#define MAX_CONN		5
#define MAX_THREADS		1 // Set to 30

// Array sizes
#define QUEUE_SIZE		10000
#define MSG_BUF_SIZE	1000000

// Comment out to disable debug messages
#define DEBUG			1

// Multithreading and thread safety
pthread_mutex_t count_mutex;
std::queue<int>* socketQ;
sem_t job_queue_count;
int server_s;
pthread_t* pool;
int currPoolSize;

// Custom error handling
void error(const char *err) {
	perror(err);
	exit(EXIT_SUCCESS);
}

// Function definitions
void cleanup();

#endif /* P2_H_ */
