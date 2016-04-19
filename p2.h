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

#define CONNECTION_INFO 1
#define BUF_SIZE 1024
#define SERVER 1
#define SERVER_PORT 1
#define CLIENT_PORT 2
#define MAXCONN 10
#define MAX_THREADS 2
#define QUEUE_SIZE 1000

// Set to 1 to enable debug messages
#define DEBUG 1

// Multithreading and thread safety
pthread_mutex_t count_mutex;
std::queue<int> socketQ;
sem_t job_queue_count;

/****************************/

// Controls main loops
int done = 0;

// Custom error handling
void error(const char *err) {
	perror(err);
	exit(EXIT_SUCCESS);
}

#endif /* P2_H_ */
