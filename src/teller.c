#include "bank.h"
#include <sys/prctl.h>

extern SharedData *shared_data;
extern sem_t *sem;

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
    
    int found = 0;
    for (int i = 0; i < shared_data->count; i++) {
        printf("%d - %d", shared_data->accounts[i].id, req->account_id);
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {
            shared_data->accounts[i].balance += req->amount;
            int client_num = get_client_number(shared_data->accounts[i].id);
            printf("Client%02d deposited %d credits… updating log\n", client_num, req->amount);
            write_log(shared_data->accounts[i].id, 'D', req->amount, shared_data->accounts[i].balance);
            found = 1;
            break;
        }
    }
    printf(" - %d",  req->account_id);
    if (!found && strcmp(req->account_id, "NEW") == 0) {
        // Assign client_num under semaphore
        int new_client_num = shared_data->count + 1;
        if (new_client_num >= MAX_ACCOUNTS) {
            fprintf(stderr, "Maximum accounts reached!\n");
            sem_post(sem);
            exit(1);
        }
        sprintf(shared_data->accounts[shared_data->count].id, "BankID_%02d", new_client_num);
        shared_data->accounts[shared_data->count].balance = req->amount;
        shared_data->count++; // Atomic increment
        printf("Client%02d served.. %s\n", new_client_num, shared_data->accounts[shared_data->count - 1].id);
        write_log(shared_data->accounts[shared_data->count - 1].id, 'D', req->amount, req->amount);
    }
    sem_post(sem);
}

void withdraw(void *arg) {
    Request *req = (Request*)arg;
    sem_wait(sem);
    int success = 0;
    
    for (int i = 0; i < shared_data->count; i++) {
        printf("%d - %d", shared_data->accounts[i].id, req->account_id);
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {
            if (shared_data->accounts[i].balance >= req->amount) {
                int client_num = get_client_number(shared_data->accounts[i].id);
                shared_data->accounts[i].balance -= req->amount;
                printf("Client%02d withdraws %d credits… updating log… ", client_num, req->amount);
                if (shared_data->accounts[i].balance == 0) {
                    printf("Bye Client%02d\n", client_num);
                    memmove(&shared_data->accounts[i], &shared_data->accounts[i+1], 
                           (shared_data->count - i - 1) * sizeof(Account));
                    shared_data->count--;
                }
                write_log(shared_data->accounts[i].id, 'W', req->amount, shared_data->accounts[i].balance);
                success = 1;
            }
            break; // Exit loop after processing
        }
    }

    if (!success) {
        int client_num = get_client_number(req->account_id);
        printf("Client%02d withdraws %d credit.. operation not permitted.\n", client_num, req->amount);
    }
    sem_post(sem);
}