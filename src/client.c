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
    
    printf("%d clients to connect.. creating clients..\n", cmd_count);
    printf("Connected to Adabank..\n");

    int client_num = 1;
    while (fgets(line, sizeof(line), file)) {
        Request req;
        char id[20], action[10];
        sscanf(line, "%s %s %d", id, action, &req.amount);
        
        strcpy(req.account_id, ((strcmp(id, "N") == 0) || strcmp(id, "BankID_None")==0) ? "NEW" : id);
        strcpy(req.action, action);
        
        int server_fd = open(SERVER_FIFO, O_WRONLY);
        write(server_fd, &req, sizeof(Request));
        close(server_fd);
        
        printf("Client%02d connected..%s %d credits\n", 
              client_num, action, req.amount);
        client_num++;
    }
    
    fclose(file);
    printf("exiting..\n");
    return 0;
}