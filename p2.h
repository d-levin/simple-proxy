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

#define BUF_SIZE 1024
#define SERVER_PORT 1
#define MAX_CONN 5
#define MAX_THREADS 1
#define QUEUE_SIZE 1000

// Set to 1 to enable debug messages
#define DEBUG 1

// Multithreading and thread safety
pthread_mutex_t count_mutex;
std::queue<int>* socketQ;
std::queue<struct sockaddr_in>* clientQ;
sem_t job_queue_count;
unsigned int num_threads;
int server_s;
pthread_t consumer;

// Custom error handling
void error(const char *err) {
	perror(err);
	exit(EXIT_SUCCESS);
}

// Function definitions
void cleanup();

#endif /* P2_H_ */
