#include "bank.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: BankClient <client_file> #ServerFIFO_Name\n");
        exit(1);
    }

    printf("Reading %s..\n", argv[1]);
    FILE *file = fopen(argv[1], "r");
    char line[256];
    int cmd_count = 0;
    
    while (fgets(line, sizeof(line), file)) cmd_count++;
    rewind(file);

    // Open FIFO write mutex
    sem_t *mutex = sem_open(FIFO_MUTEX, 0);
    if (mutex == SEM_FAILED) {
        perror("sem_open (client mutex)");
        exit(1);
    }
    
    printf("%d clients to connect.. creating clients..\n", cmd_count);
    printf("Connected to Adabank..\n");

    int client_num = 1;
    while (fgets(line, sizeof(line), file)) {
        Request req;
        char id[20], action[10];
        sscanf(line, "%s %s %d", id, action, &req.amount);
        
        strcpy(req.account_id, (strcmp(id, "N") == 0) ? "NEW" : id);
        strcpy(req.action, action);
        
        // Lock mutex before writing to FIFO
        if (sem_wait(mutex) == -1) {
            perror("sem_wait");
            break;
        }

        int server_fd = open(argv[2], O_WRONLY);
        write(server_fd, &req, sizeof(Request));
        close(server_fd);

        // Unlock mutex
        if (sem_post(mutex) == -1) {
            perror("sem_post");
            break;
        }
        
        printf("Client%02d connected..%s %d credits\n", 
              client_num, action, req.amount);
        client_num++;
    }
    
    fclose(file);
    sem_t *sem = sem_open(REQ_SEM, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open (client)");
        exit(1);
    }
    sem_post(sem);  // Signal server
    sem_close(sem);
    sem_close(mutex);
    printf("exiting..\n");
    return 0;
}