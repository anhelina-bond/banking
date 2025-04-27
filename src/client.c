#include "bank.h"
#include <sys/stat.h>
extern sem_t *fifo_mutex;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client_file> <server_fifo>\n", argv[0]);
        exit(1);
    }

    int server_fd = open(argv[2], O_WRONLY );
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
        sem_wait(fifo_mutex);
        write(server_fd, &req, sizeof(Request));
        sem_post(fifo_mutex);

        printf("Client%02d connected..%s %d credits\n", client_num, action, req.amount);
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
    for (int i = 0; i < cmd_count; i++) {
        int resp_fd = open(client_fifo, O_RDONLY);
        char response[256];
        ssize_t bytes_read = read(resp_fd, response, sizeof(response));
        if (bytes_read > 0) {
            printf("%.*s\n", (int)bytes_read, response);
        } else {
            perror("read");
        }
        close(resp_fd);
    }
    unlink(client_fifo);
    
    sem_close(fifo_mutex);
    printf("exiting..\n");
    return 0;
}