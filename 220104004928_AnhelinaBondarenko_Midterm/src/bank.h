#ifndef BANK_H
#define BANK_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "errno.h"

#define SERVER_FIFO "/tmp/bank_server.fifo"
#define MAX_ACCOUNTS 100
#define SHM_NAME "/bank_shm"
#define SEM_NAME "/bank_sem"
#define REQ_SEM "/bank_req_sem"
#define FIFO_MUTEX "/bank_fifo_mutex" // Named semaphore for FIFO write lock

typedef struct {
    char id[20];
    int client_id;
    int balance;
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int count;
    int client_count;
} SharedData;

typedef struct {
    char action[10];    // "deposit" or "withdraw"
    char account_id[20];
    int amount;
    char client_fifo[500];
} Request;

// Custom Teller process functions
pid_t Teller(void (*func)(void*), void* arg);
int waitTeller(pid_t pid, int *status);

// Teller handlers
void deposit(void *arg);
void withdraw(void *arg);

extern SharedData *shared_data;
extern sem_t *sem;
extern sem_t *fifo_mutex;
void handle_signal(int sig);
void write_log(const char *id, char type, int amount, int balance);
int handle_client(Request *req);


#endif