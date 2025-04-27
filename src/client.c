#include "bank.h"
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client_file> <server_fifo>\n", argv[0]);
        exit(1);
    }

    int server_fd = open(argv[2], O_WRONLY | O_NONBLOCK);
    if (server_fd == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "Error: Server is not running.\n");
            exit(1);
        } else {
            perror("open");
            exit(1);
        }
    }

    // Create client-specific FIFO
    char client_fifo[50];
    snprintf(client_fifo, sizeof(client_fifo), "/tmp/client_%d", getpid());
    printf("Client fifo name: %s\n", client_fifo);
    mkfifo(client_fifo, 0666); // Create FIFO

    printf("Reading %s..\n", argv[1]);
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("fopen");
        unlink(client_fifo);
        exit(1);
    }

    char line[256];
    int cmd_count = 0;
    
    while (fgets(line, sizeof(line), file)) cmd_count++;
    rewind(file);

    // Open server FIFO
    sem_t *mutex = sem_open(FIFO_MUTEX, 0);
    if (mutex == SEM_FAILED) {
        perror("sem_open (client mutex)");
        unlink(client_fifo);
        exit(1);
    }
    sem_t *req_sem = sem_open(REQ_SEM, 0);
    if (req_sem == SEM_FAILED) {
        perror("sem_open (client)");
        exit(1);
    }

    printf("%d clients to connect.. creating clients..\n", cmd_count);
    printf("Connected to Adabank..\n");
    
    int client_num = 1;
    while (fgets(line, sizeof(line), file)) {
        Request req;
        char id[20], action[10];
        sscanf(line, "%s %s %d", id, action, &req.amount);
        
        // Populate request
        strcpy(req.account_id, (strcmp(id, "N") == 0) ? "NEW" : id);
        strcpy(req.action, action);
        strncpy(req.client_fifo, client_fifo, sizeof(req.client_fifo));
        req.client_sequence = client_num; // Track request order

        // Send request to server
        sem_wait(mutex);
        write(server_fd, &req, sizeof(Request));
        sem_post(mutex);

        client_num++;
    }
    fclose(file);
    close(server_fd);
    
    sem_post(req_sem);  // Signal server
    sem_close(req_sem);


    //* TODO: first read the data written by handle_client(), using semaphore(maybe??)
    // Then read the result:
    // After sending all requests
    // Read responses from client FIFO
    printf("Waiting for responses...\n");
    int resp_fd = open(client_fifo, O_RDONLY); // Open once
    if (resp_fd == -1) {
        perror("open");
        unlink(client_fifo);
        exit(1);
    }

    // Read all responses in a single session
    char buffer[256];
    int expected_responses = cmd_count * 2; // 2 responses per command
    int received = 0;

    while (received < expected_responses) {
        ssize_t bytes = read(resp_fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            printf("%.*s\n", (int)bytes, buffer);
            received++;
        } else if (bytes == 0) {
            break; // EOF (server closed FIFO)
        } else {
            perror("read");
            break;
        }
    }

    close(resp_fd);
    
    // for (int i = 0; i < cmd_count*2; i++) {
    //     int resp_fd = open(client_fifo, O_RDONLY);
    //     char response[256];
    //     ssize_t bytes_read = read(resp_fd, response, sizeof(response));
    //     if (bytes_read > 0) {
    //         printf("%.*s\n", (int)bytes_read, response);
    //     }
    //     close(resp_fd);
    // }
    unlink(client_fifo);
    
    sem_close(mutex);
    printf("exiting..\n");
    return 0;
}