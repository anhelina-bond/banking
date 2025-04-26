#include "bank.h"
#include <signal.h>
#include <time.h>

SharedData *shared_data;
sem_t *sem;
sem_t *req_sem;
sem_t *fifo_mutex;
int server_fd;
const char* LOG_FILE = "AdaBank.bankLog";


// Extract client number from BankID_XX (e.g., "BankID_02" → 2)
int get_client_number(const char *account_id) {
    if (strncmp(account_id, "BankID_", 7) != 0) return -1;
    return atoi(account_id + 7); // Extract numeric part after "BankID_"
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
    ftruncate(shm_fd, sizeof(SharedData));
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    shared_data->count = 0;

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
        printf("Waiting for clients @%s…\n", SERVER_FIFO);
        fflush(stdout);
        
        sem_wait(req_sem);
        Request req_buffer[10]; // Max batch size 10
        int batch_count = 0;
        while (1) {
            Request req;
            ssize_t bytes_read = read(server_fd, &req, sizeof(Request));
            
            if (bytes_read == sizeof(Request)) {
                req_buffer[batch_count++] = req;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break; // FIFO empty
                perror("read");
                break;
            }
        }

        if (batch_count > 0) {
            printf("-- Received %d clients from PIDClientX..\n", batch_count);
            // char **teller_msgs = malloc(batch_count * sizeof(char*));

            for (int i = 0; i < batch_count; i++) {
                Request *req = &req_buffer[i];
                int client_num;

                // Lock semaphore before reading shared_data->count
                sem_wait(sem);
                if (strcmp(req->account_id, "NEW") == 0) {
                    // New client
                    client_num = shared_data->count + 1;
                } else {
                    // Existent client
                    client_num = get_client_number(req->account_id);
                }
                sem_post(sem); // Unlock immediately after reading

                // Fork Teller and process request
                pid_t tid = Teller(strcmp(req->action, "deposit") == 0 ? deposit : withdraw, &req);
                printf( "-- Teller PID%d is active serving Client%02d…\n", tid, client_num);
                waitpid(tid, NULL, 0); // Wait for Teller to finish
            }

            // Print all buffered Teller messages
            // for (int i = 0; i < batch_count; i++) {
            //     printf("%s", teller_msgs[i]);
            //     free(teller_msgs[i]);
            // }
            // free(teller_msgs);
        }
    }
}

