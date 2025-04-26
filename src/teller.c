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
    if(strcmp(req->account_id, "NEW") == 0) {
        // Assign client_num under semaphore
            int new_client_num = shared_data->count + 1;
            if (new_client_num >= MAX_ACCOUNTS) {
                fprintf(stderr, "Maximum accounts reached!\n");
                sem_post(sem);
                exit(1);
            }
            sprintf(shared_data->accounts[shared_data->count].id, "BankID_%02d", new_client_num);
            shared_data->accounts[shared_data->count].balance = req->amount;
            shared_data->count +=1; // Atomic increment
            printf("Client%02d deposited %d credits… updating log\n", new_client_num, req->amount);
            write_log(shared_data->accounts[shared_data->count - 1].id, 'D', req->amount, req->amount);
            printf("[DEPOSIT] Created BankID_%02d. New count: %d\n", new_client_num, shared_data->count);
    } else {
        printf("[DEBUG] Current account count: %d\n", shared_data->count);
        for (int i = 0; i < shared_data->count; i++) {
            if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {
                shared_data->accounts[i].balance += req->amount;
                int client_num = get_client_number(shared_data->accounts[i].id);
                printf("Client%02d deposited %d credits… updating log\n", client_num, req->amount);
                write_log(shared_data->accounts[i].id, 'D', req->amount, shared_data->accounts[i].balance);
                found = 1;
                break;
            }
        }
        if(!found) {
            int client_num = get_client_number(req->account_id);
            printf("Client%02d deposits %d credit.. invalid ID.\n", client_num, req->amount);
            shared_data->count +=1; // Atomic increment
        }
        

    }    

    free(req); // Release memory
    sem_post(sem);
}

void withdraw(void *arg) {
    Request *req = (Request*)arg;
    sem_wait(sem);
    int success = 0;
    printf("[DEBUG] Current account count: %d\n", shared_data->count);
    for (int i = 0; i < shared_data->count; i++) {
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0) {            // client found in database
            if (shared_data->accounts[i].balance >= req->amount) {                  // requested amount is valid
                int client_num = get_client_number(shared_data->accounts[i].id);
                shared_data->accounts[i].balance -= req->amount;
                printf("Client%02d withdraws %d credits… updating log…\n ", client_num, req->amount);
                
                write_log(shared_data->accounts[i].id, 'W', req->amount, shared_data->accounts[i].balance);

                if (shared_data->accounts[i].balance == 0) {
                    printf("Bye Client%02d\n", client_num);
                    memmove(&shared_data->accounts[i], &shared_data->accounts[i+1], 
                           (shared_data->count - i - 1) * sizeof(Account));
                }
                success = 1;
                printf("[WITHDRAW] Updated count: %d\n", shared_data->count);
            }
            break; // Exit loop after processing
        }
    }

    if (!success) {
        int client_num = get_client_number(req->account_id);
        printf("Client%02d withdraws %d credit.. operation not permitted.\n", client_num, req->amount);
        shared_data->count +=1; // Atomic increment
    }

    free(req); // Release memory
    sem_post(sem);
}