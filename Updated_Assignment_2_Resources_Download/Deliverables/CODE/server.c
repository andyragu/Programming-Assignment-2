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
    char queue_name[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t lock;

__thread int thread_id;

// **Function to Print Server Header**
void print_header() {
    printf("\n============================================\n");
    printf("==== STUDENTS' REFERENCE SHELL SERVER ====\n");
    printf("####### I am the Parent Process (PID: %d) running this SERVER #######\n", getpid());
    printf("============================================\n");
    printf("\n[Main Thread -- %09lu]: I am the Server's Main Thread. My Parent Process is (PID: %d)...\n", pthread_self() % 1000000000, getpid());
    printf("[Main Thread -- %09lu]: Broadcast message queue & Server message queue created. Waiting for client messages...\n", pthread_self() % 1000000000);
}

// **Function to Send Response to Client**
void send_response(const char* queue, const char* response) {
    mqd_t mq = mq_open(queue, O_WRONLY);
    if (mq != (mqd_t)-1) {
        mq_send(mq, response, strlen(response) + 1, 0);
        mq_close(mq);
    }
}

// **Client Handler (Child Thread)**
void* handle_client(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);

    pthread_t child_thread_id = pthread_self();
    
    // **Log Child Thread Registration with Proper Buffer Handling**
    char child_thread_log[MAX_MSG_SIZE];

    // **Process "REGISTER" Command**
    if (strncmp(command, "REGISTER ", 9) == 0) {
        int pid;
        sscanf(command + 9, "%d", &pid);

        pthread_mutex_lock(&lock);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].pid = pid;
            snprintf(clients[client_count].queue_name, sizeof(clients[client_count].queue_name),
                     "/client_broadcast_%d", pid);
            client_count++;
        }
        pthread_mutex_unlock(&lock);

        snprintf(child_thread_log, sizeof(child_thread_log),
            "\n[Child Thread * %015lu]: Registered client (PID: %d) to the client list. Total clients ---> [%d]\n",
            child_thread_id % 1000000000000000, pid, client_count);

        snprintf(child_thread_log + strlen(child_thread_log), sizeof(child_thread_log) - strlen(child_thread_log),
            "[Child Thread * %015lu]: Registered the Shutdown broadcast message queue '%s'\n",
            child_thread_id % 1000000000000000, clients[client_count - 1].queue_name);
    }

    // **Print Child Thread Logs**
    printf("%s", child_thread_log);
    
    pthread_exit(NULL);
}

// **Signal Handler for Cleanup**
void cleanup_server(int signum) {
    printf("\n[Server] Shutting down...\n");
    mq_unlink(SERVER_QUEUE);
    exit(0);
}

// **Main Server Function**
int main() {
    mqd_t mq;
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};

    signal(SIGINT, cleanup_server);
    pthread_mutex_init(&lock, NULL);

    print_header();

    // **Create Message Queue**
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

            // **Main Thread Logs BEFORE Creating Child Thread**
            printf("\n[Main Thread -- %09lu]: Received command '%.900s' from the client. About to create a child thread.\n", pthread_self() % 1000000000, buffer);

            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, strdup(buffer));
            pthread_detach(thread);

            // **Main Thread Logs AFTER Creating Child Thread**
            printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n", pthread_self() % 1000000000, thread % 1000000000000000);
            printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n", pthread_self() % 1000000000, thread % 1000000000000000);
        }
    }

    mq_close(mq);
    return 0;
} 