#include "bank.h"
#include <signal.h>
#include <time.h>

SharedData *shared_data;
sem_t *sem;
int server_fd;
int client_counter = 0;
const char* LOG_FILE = "AdaBank.bankLog";

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
    printf("Removing ServerFIFO… Updating log file…\n");
    printf("Adabank says \"Bye\"...\n");
    exit(0);
}

void handle_signal(int sig) {
    printf("\nSignal received closing active Tellers\n");
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
    server_fd = open(SERVER_FIFO, O_RDONLY);

    // Shared memory setup
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedData));
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    shared_data->count = 0;

    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);

    printf("Waiting for clients @%s…\n", SERVER_FIFO);
    
    while (1) {
        Request req;
        if (read(server_fd, &req, sizeof(Request)) > 0) {
            client_counter++;
            printf("-- Received %d clients from PIDClientX..\n", client_counter);
            
            pid_t tid = Teller(strcmp(req.action, "deposit") == 0 ? deposit : withdraw, &req);
            printf("-- Teller PID%d is active serving Client%02d…\n", tid, client_counter);
            
            waitTeller(tid, NULL);
        }
    }
}