#include "..\include\bank.h"
#include <sys/prctl.h>

pid_t Teller(void (*func)(void*), void *arg) {
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM); // Terminate if parent dies
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
    
    // Find or create account
    int found = 0;
    for (int i = 0; i < shared_data->count; i++) {
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {
            shared_data->accounts[i].balance += req->amount;
            printf("Client%02d deposited %d credits… updating log\n", client_counter, req->amount);
            write_log(shared_data->accounts[i].id, 'D', req->amount, shared_data->accounts[i].balance);
            found = 1;
            break;
        }
    }
    
    if (!found && strcmp(req->account_id, "NEW") == 0) {
        // Create new account
        sprintf(shared_data->accounts[shared_data->count].id, "BankID_%02d", shared_data->count+1);
        shared_data->accounts[shared_data->count].balance = req->amount;
        printf("Client%02d served.. %s\n", client_counter, shared_data->accounts[shared_data->count].id);
        write_log(shared_data->accounts[shared_data->count].id, 'D', req->amount, req->amount);
        shared_data->count++;
    }
    sem_post(sem);
}

void withdraw(void *arg) {
    Request *req = (Request*)arg;
    sem_wait(sem);
    int success = 0;
    
    for (int i = 0; i < shared_data->count; i++) {
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {
            if (shared_data->accounts[i].balance >= req->amount) {
                shared_data->accounts[i].balance -= req->amount;
                printf("Client%02d withdraws %d credits… updating log… ", client_counter, req->amount);
                if (shared_data->accounts[i].balance == 0) {
                    printf("Bye Client%02d\n", client_counter);
                    // Remove account
                    memmove(&shared_data->accounts[i], &shared_data->accounts[i+1], 
                           (shared_data->count - i - 1) * sizeof(Account));
                    shared_data->count--;
                }
                write_log(shared_data->accounts[i].id, 'W', req->amount, shared_data->accounts[i].balance);
                success = 1;
            }
            break;
        }
    }
    
    if (!success) {
        printf("Client%02d withdraws %d credit.. operation not permitted.\n", client_counter, req->amount);
    }
    sem_post(sem);
}