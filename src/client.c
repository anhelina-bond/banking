#include "bank.h"
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <client_file> <server_fifo>\n", argv[0]);
        exit(1);
    }

    // Create client-specific FIFO
    char client_fifo[50];
    snprintf(client_fifo, sizeof(client_fifo), "/tmp/client_%d", getpid());
    mkfifo(client_fifo, 0666); // Create FIFO

    printf("Reading %s..\n", argv[1]);
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("fopen");
        unlink(client_fifo);
        exit(1);
    }

    // Open server FIFO
    sem_t *mutex = sem_open(FIFO_MUTEX, 0);
    if (mutex == SEM_FAILED) {
        perror("sem_open (client mutex)");
        unlink(client_fifo);
        exit(1);
    }

    int client_num = 1;
    char line[256];
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
        int server_fd = open(argv[2], O_WRONLY);
        write(server_fd, &req, sizeof(Request));
        close(server_fd);
        sem_post(mutex);

        printf("Client%02d connected..%s %d credits\n", client_num, action, req.amount);
        client_num++;
    }
    fclose(file);

    // Read responses from client FIFO
    int resp_fd = open(client_fifo, O_RDONLY);
    char response[256];
    ssize_t bytes_read;
    while ((bytes_read = read(resp_fd, response, sizeof(response))) > 0) {
        printf("%.*s\n", (int)bytes_read, response);
    }
    close(resp_fd);
    unlink(client_fifo); // Cleanup FIFO
    sem_close(mutex);
    return 0;
}