#include "bank.h"
#include <sys/prctl.h>

extern SharedData *shared_data;
extern sem_t *sem;
extern sem_t *fifo_mutex;

pid_t Teller(void (*func)(void*), void *arg) {
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        func(arg); 
        exit(0);
    }
    return pid;
}

int waitTeller(pid_t pid, int *status) {
    return waitpid(pid, status, 0);
}

void deposit(void *arg) {
    Request *req = (Request*)arg;
    sem_wait(sem);
    char response[256];
    int success = 0;
    
    if(strcmp(req->account_id, "NEW") == 0) {
        // Assign client_num under semaphore
        int new_client_num = shared_data->count + 1;
        if (new_client_num >= MAX_ACCOUNTS) {
            printf("Client%02d: Deposit failed. Maximum accounts reached!", shared_data->client_count);
            snprintf(response, sizeof(response), 
                "--Client%02d: Deposit failed. Maximum accounts reached!", shared_data->client_count);
        } else {
            sprintf(shared_data->accounts[shared_data->count].id, "BankID_%02d", new_client_num);
            shared_data->accounts[shared_data->count].balance = req->amount;
            
            printf("--Client%02d: Deposited %d credits. New account: %s... updating log", 
                shared_data->client_count, req->amount, shared_data->accounts[shared_data->count].id);
            snprintf(response, sizeof(response), 
                "--Client%02d served.. %s", 
                shared_data->client_count,  shared_data->accounts[shared_data->count].id);
            success = 1;
            write_log(shared_data->accounts[shared_data->count].id, 'D', req->amount, req->amount);
            shared_data->count +=1; // Atomic increment
        }
    } else {
        // Case 2/3: Existing or invalid account
        for (int i = 0; i < shared_data->count; i++) {
            if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {
                shared_data->accounts[i].balance += req->amount;
                printf("Client%02d: Deposited %d credits. New balance: %d... updating log", 
                    shared_data->accounts[i].client_id, req->amount, shared_data->accounts[i].balance);
                snprintf(response, sizeof(response), 
                    "Client%02d served.. New balance: %d", 
                    shared_data->accounts[i].client_id, shared_data->accounts[i].balance);
                success = 1;
                write_log(shared_data->accounts[i].id, 'D', req->amount, shared_data->accounts[i].balance);
                break;
            }
        }
        if(!success) {
            printf("Client%02d: Deposit failed. Invalid account: %s", 
                shared_data->client_count, req->account_id);
            snprintf(response, sizeof(response), 
                "Client%02d: Deposit failed. Invalid account: %s", 
                shared_data->client_count, req->account_id);
        }
    }    

    // Write response to client FIFO
    sem_wait(fifo_mutex);
    int client_fd = open(req->client_fifo, O_WRONLY);
    if (client_fd != -1) {
        write(client_fd, response, strlen(response) + 1);  // Include null terminator
        close(client_fd);
    }
    sem_post(fifo_mutex);
    sem_post(sem);
    free(req);
}

void withdraw(void *arg) {
    Request *req = (Request*)arg;
    sem_wait(sem);
    char response[256];
    int success = 0;   

    for (int i = 0; i < shared_data->count; i++) {
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {            // client found in database
            if (shared_data->accounts[i].balance >= req->amount) {                  // requested amount is valid
                shared_data->accounts[i].balance -= req->amount;
                write_log(shared_data->accounts[i].id, 'W', req->amount, shared_data->accounts[i].balance);
                if (shared_data->accounts[i].balance == 0) {
                    printf("Client%02d: Withdrew %d credits. Account closed... updating log", 
                        shared_data->accounts[i].client_id, req->amount);
                    snprintf(response, sizeof(response), 
                        "Client%02d served.. account closed", 
                        shared_data->accounts[i].client_id);
                    // Remove account
                    memmove(&shared_data->accounts[i], &shared_data->accounts[i + 1], 
                           (shared_data->count - i - 1) * sizeof(Account));
                } else {
                    snprintf(
                        "Client%02d: Withdrew %d credits. New balance: %d... updating log", 
                        shared_data->accounts[i].client_id, req->amount, shared_data->accounts[i].balance);
                    snprintf(response, sizeof(response), 
                        "Client%02d served.. New balance: %d... updating log", 
                        shared_data->accounts[i].client_id,  shared_data->accounts[i].balance);
                }
                success = 1;
                break;
            }
        }
    }

    if (!success) {
        printf( "Client%02d: Withdrawal failed. Invalid operation.", shared_data->client_count);
        snprintf(response, sizeof(response), 
            "Client%02d: Withdrawal failed. Invalid operation.", shared_data->client_count);
    }
    // Write response to client FIFO
    sem_wait(fifo_mutex);
    int client_fd = open(req->client_fifo, O_WRONLY);
    if (client_fd != -1) {
        write(client_fd, response, strlen(response) + 1);  // Include null terminator
        close(client_fd);
    }
    sem_post(fifo_mutex);
    sem_post(sem);
    free(req);
}