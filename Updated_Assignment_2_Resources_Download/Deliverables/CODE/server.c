#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#define SERVER_QUEUE "/server_queue"
#define MAX_MSG_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    int pid;
    int hidden;
    char queue_name[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t lock;

// 📌 Helper function to print the header like in the instructor's output
void print_header() {
    printf("\n============================================\n");
    printf("==== STUDENT REFERENCE SHELL SERVER ====\n");
    printf("============================================\n");
    printf("[Main Thread -- %ld]: I am the Server's Main Thread. My Parent Process is (PID: %d)...\n", pthread_self(), getpid());
    printf("[Main Thread -- %ld]: Broadcast message queue & Server message queue created. Waiting for client messages...\n", pthread_self());
}

// 📌 Function to send messages back to clients
void send_response(const char* queue, const char* response) {
    mqd_t mq = mq_open(queue, O_WRONLY);
    if (mq != (mqd_t)-1) {
        mq_send(mq, response, strlen(response) + 1, 0);
        mq_close(mq);
    }
}

// 📌 Handle client requests in a new thread
void* handle_client(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);

    printf("[Main Thread -- %ld]: Received command '%s'. About to create a child thread.\n", pthread_self(), command);

    pthread_t child_thread = pthread_self();
    printf("[Main Thread -- %ld]: Successfully created the child thread [%ld]\n", pthread_self(), child_thread);

    if (strncmp(command, "REGISTER ", 9) == 0) {
        int pid;
        sscanf(command + 9, "%d", &pid);

        pthread_mutex_lock(&lock);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].pid = pid;
            clients[client_count].hidden = 0;
            sprintf(clients[client_count].queue_name, "/client_broadcast_%d", pid);
            client_count++;
        }
        pthread_mutex_unlock(&lock);

        printf("[Child Thread * %ld]: Registered client (PID: %d) to the client list. Total clients ---> [%d]\n", child_thread, pid, client_count);
        printf("[Child Thread * %ld]: Registered the Shutdown broadcast message queue '%s'\n", child_thread, clients[client_count - 1].queue_name);
    } 
    else if (strcmp(command, "LIST") == 0) {
        char response[MAX_MSG_SIZE] = "Clients:\n";
        pthread_mutex_lock(&lock);
        for (int i = 0; i < client_count; i++) {
            if (!clients[i].hidden) {
                char temp[50];
                sprintf(temp, "Client %d --> (PID %d)\n", i + 1, clients[i].pid);
                strcat(response, temp);
            }
        }
        pthread_mutex_unlock(&lock);
        send_response(SERVER_QUEUE, response);
    } 
    else if (strcmp(command, "HIDE") == 0) {
        clients[client_count - 1].hidden = 1;
        send_response(SERVER_QUEUE, "You are now hidden...");
    } 
    else if (strcmp(command, "UNHIDE") == 0) {
        clients[client_count - 1].hidden = 0;
        send_response(SERVER_QUEUE, "You are now visible...");
    } 
    else {
        FILE* fp = popen(command, "r");
        if (fp == NULL) {
            send_response(SERVER_QUEUE, "Error: Invalid command.");
        } else {
            char output[MAX_MSG_SIZE] = {0};
            fread(output, sizeof(char), MAX_MSG_SIZE - 1, fp);
            pclose(fp);
            send_response(SERVER_QUEUE, output);
        }
    }

    printf("[Main Thread -- %ld]: The child thread [%ld] successfully exited\n", pthread_self(), child_thread);
    pthread_exit(NULL);
}

// 📌 Handle SIGINT (CTRL+C) to clean up message queues
void cleanup_server(int signum) {
    printf("\n[Server] Shutting down...\n");
    mq_unlink(SERVER_QUEUE);
    exit(0);
}

// 📌 Main server function
int main() {
    mqd_t mq;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MSG_SIZE;

    signal(SIGINT, cleanup_server);
    pthread_mutex_init(&lock, NULL);

    print_header();

    mq = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    while (1) {
        char buffer[MAX_MSG_SIZE];
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read >= 0) {
            buffer[bytes_read] = '\0';
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, strdup(buffer));
            pthread_detach(thread);
        }
    }

    mq_close(mq);
    return 0;
}
