#include "bank.h"
#include <signal.h>
#include <time.h>
#include <ctype.h>

SharedData *shared_data;
sem_t *sem;
sem_t *req_sem;
sem_t *fifo_mutex;
int server_fd;
const char* LOG_FILE = "AdaBank.bankLog";


int get_client_number(const char *account_id) {
    if (strncmp(account_id, "BankID_", 7) != 0) {
        // Invalid format: treat as new client and increment client_count
        return (++shared_data->client_count);
    }
    const char *num_part = account_id + 7;
    for (int i = 0; num_part[i] != '\0'; i++) {
        if (!isdigit(num_part[i])) {
            // Invalid characters: treat as new client
            return (++shared_data->client_count);
        }
    }
    // Valid existing account: parse the client number from the ID
    return atoi(num_part);
}

int handle_client(Request *req) {
    char response[256];
    static int client_fd = -1; // Persistent descriptor for the client FIFO
    int client_num;
    // Open FIFO once per client session
    if (client_fd == -1) {
        client_fd = open(req->client_fifo, O_WRONLY); // No O_APPEND needed
        if (client_fd == -1) {
            perror("open");
            sem_post(sem);
            free(req);
            return;
        }
    }
    sem_wait(sem);
    
    // New client
    if (strcmp(req->account_id, "NEW")) {
        shared_data->client_count += 1;
        shared_data->accounts[shared_data->count].client_id = shared_data->client_count;
        client_num = shared_data->client_count;
    }

    // Check if account ID is in db
    for (int i = 0; i < shared_data->count; i++) {
        if (strcmp(shared_data->accounts[i].id, req->account_id) == 0){
            client_num = shared_data->accounts[i].client_id;
            break;
        }
    }
    // Client not found or invalid ID
    shared_data->client_count += 1;
    client_num = shared_data->client_count;
    snprintf(response, sizeof(response), "Client%02d connected..%s %d credits\n", client_num, req->action, req->amount);
    write(client_fd, response, strlen(response) + 1);
    sem_post(sem);
}


void write_log(const char *id, char type, int amount, int balance) {
    FILE *log = fopen(LOG_FILE, "a");
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "# %s %c %d %d @%02d:%02d %s %d\n",
            id, type, amount, balance,
            t->tm_hour, t->tm_min, 
            t->tm_mon < 12 ? "April" : "Unknown",  // Simplified for example
            t->tm_mday);
    fclose(log);
}

void cleanup() {
    printf("\nSignal received closing active Tellers\n");
    munmap(shared_data, sizeof(SharedData));
    shm_unlink(SHM_NAME);
    sem_close(sem);
    sem_unlink(SEM_NAME);
    unlink(SERVER_FIFO);
    sem_close(req_sem);
    sem_unlink(REQ_SEM);
    sem_close(fifo_mutex);
    sem_unlink(FIFO_MUTEX);
    printf("Removing ServerFIFO… Updating log file…\n");
    printf("Adabank says \"Bye\"...\n");
    exit(0);
}

void handle_signal(int sig) {
    (void)sig;
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "AdaBank") != 0) {
        printf("Usage: BankServer AdaBank #ServerFIFO_Name\n");
        exit(1);
    }

    signal(SIGINT, handle_signal);
    
    // Initialize bank
    printf("Adabank is active...\n");
    if (access(LOG_FILE, F_OK) == -1) {
        printf("No previous logs.. Creating the bank database\n");
    }

    // Create server FIFO
    mkfifo(SERVER_FIFO, 0666);
    server_fd = open(SERVER_FIFO, O_RDONLY | O_NONBLOCK);

    // Shared memory setup
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate failed");
        exit(1);
    }
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    // Explicitly initialize count to 0
    shared_data->count = 0;
    shared_data->client_count = 0;

    // Semaphores setup
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    req_sem = sem_open(REQ_SEM, O_CREAT, 0666, 0);
    if (req_sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    sem_unlink(FIFO_MUTEX); // Cleanup previous instances
    fifo_mutex = sem_open(FIFO_MUTEX, O_CREAT | O_EXCL, 0666, 1);
    if (fifo_mutex == SEM_FAILED) {
        perror("sem_open (fifo_mutex)");
        exit(1);
    }

    
    while (1) {
        printf("\n. . .\nWaiting for clients @%s…\n", SERVER_FIFO);
        fflush(stdout);
        
        sem_wait(req_sem);
        Request *req_buffer[20]; // Max batch size 10
        int batch_count = 0;
        while (batch_count < 20) {
            Request *req = malloc(sizeof(Request)); // Allocate on heap
            ssize_t bytes_read = read(server_fd, req, sizeof(Request));
            if (bytes_read == sizeof(Request)) {
                req_buffer[batch_count++] = req;
            } else {
                free(req); // Cleanup if read failed
                break;
            }
        }

        if (batch_count > 0) {
            printf("-- Received %d clients from PIDClientX..\n", batch_count);
            for (int i = 0; i < batch_count; i++) {
                Request *req = req_buffer[i];
                int client_num = handle_client(req);

                pid_t tid = Teller(strcmp(req->action, "deposit") == 0 ? deposit : withdraw, req);
                printf("-- Teller PID%d is active serving Client%02d…\n", tid, client_num);
                waitpid(tid, NULL, 0);
            }
        }
    }
}

