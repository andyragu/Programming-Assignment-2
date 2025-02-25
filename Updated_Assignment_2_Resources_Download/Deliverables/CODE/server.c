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

// Function to Print Server Header
void print_header() {
    printf("\n============================================\n");
    printf("==== STUDENTS' REFERENCE SHELL SERVER ====\n");
    printf("####### I am the Parent Process (PID: %d) running this SERVER #######\n", getpid());
    printf("============================================\n");
    printf("\n[Main Thread -- %09lu]: I am the Server's Main Thread. My Parent Process is (PID: %d)...\n",
           pthread_self() % 1000000000, getpid());
    printf("[Main Thread -- %09lu]: Broadcast message queue & Server message queue created. Waiting for the client messages...\n",
           pthread_self() % 1000000000);
}

// Function to Send Response to Client
void send_response(const char* queue, const char* response) {
    mqd_t mq = mq_open(queue, O_WRONLY);
    if (mq != (mqd_t)-1) {
        mq_send(mq, response, strlen(response) + 1, 0);
        mq_close(mq);
    }
}

// Thread to Handle LIST Command
void* handle_list(void* arg) {
    int client_pid = *((int*)arg);
    free(arg);
    char response[MAX_MSG_SIZE] = "Connected Clients:\n";
    char response_queue[50] = "";
    int found = 0;

    pthread_mutex_lock(&lock);
    // Search for the registered client's queue name and build the list
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == client_pid) {
            strcpy(response_queue, clients[i].queue_name);
            found = 1;
        }
        char client_info[50];
        snprintf(client_info, sizeof(client_info), "Client %d --> (PID %d)\n", i + 1, clients[i].pid);
        strcat(response, client_info);
    }
    pthread_mutex_unlock(&lock);

    if (!found) {
        printf("[Server] Could not find client with PID %d in client list.\n", client_pid);
        pthread_exit(NULL);
    }

    printf("[Server] Sending LIST response to client queue: %s\n", response_queue);

    mqd_t client_mq;
    for (int i = 0; i < 5; i++) {  // Retry up to 5 times
        client_mq = mq_open(response_queue, O_WRONLY);
        if (client_mq != (mqd_t)-1) {
            if (mq_send(client_mq, response, strlen(response) + 1, 0) != -1) {
                mq_close(client_mq);
                pthread_exit(NULL);
            }
            mq_close(client_mq);
        }
        usleep(100000);  // Sleep 100ms before retrying
    }

    perror("[Server] Failed to send LIST response after retries");
    pthread_exit(NULL);
}

// Client Handler (Child Thread)
void* handle_client(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);

    pthread_t child_thread_id = pthread_self();
    char queue_name[50];
    int pid = -1;

    if (strncmp(command, "REGISTER ", 9) == 0) {
        // Expected format: "REGISTER <pid> <queue_name>"
        if (sscanf(command + 9, "%d %s", &pid, queue_name) != 2) {
            printf("[Server] Invalid REGISTER command format.\n");
            pthread_exit(NULL);
        }

        pthread_mutex_lock(&lock);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].pid = pid;
            strcpy(clients[client_count].queue_name, queue_name);
            client_count++;
        }
        pthread_mutex_unlock(&lock);

        printf("\n[Child Thread * %015lu]: Registered client (PID: %d) to the client list. Total Clients ---> [%d]\n",
               child_thread_id % 1000000000000000, pid, client_count);
        printf("[Child Thread * %015lu]: Registered the Response queue '%s'\n",
               child_thread_id % 1000000000000000, queue_name);
    }

    pthread_exit(NULL);
}

// Signal Handler for Cleanup
void cleanup_server(int signum) {
    printf("\n[Server] Shutting down...\n");
    mq_unlink(SERVER_QUEUE);
    exit(0);
}

// Main Server Function
int main() {
    mqd_t mq;
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};

    signal(SIGINT, cleanup_server);
    pthread_mutex_init(&lock, NULL);

    print_header();

    // Create Server Message Queue
    mq = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    while (1) {
        char buffer[MAX_MSG_SIZE];
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        if (bytes_read < 0) {
            perror("mq_receive failed");
            continue;
        }
        buffer[bytes_read] = '\0';

        pthread_t thread;

        // Handle LIST command: format "LIST <pid>"
        if (strncmp(buffer, "LIST", 4) == 0) {
            int client_pid;
            if (sscanf(buffer, "LIST %d", &client_pid) != 1) {
                printf("[Main Thread -- %09lu]: LIST command received without client PID. Ignoring.\n", 
                       pthread_self() % 1000000000);
                continue;
            }
            int* pid_ptr = malloc(sizeof(int));
            if (!pid_ptr) {
                perror("malloc failed");
                continue;
            }
            *pid_ptr = client_pid;
            printf("\n[Main Thread -- %09lu]: Received command 'LIST' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, client_pid);

            if (pthread_create(&thread, NULL, handle_list, pid_ptr) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long)thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long)thread);
            } else {
                perror("pthread_create failed");
                free(pid_ptr);
            }
        }
        // Handle other commands (e.g., REGISTER)
        else if (strncmp(buffer, "REGISTER ", 9) == 0) {
            int client_pid;
            sscanf(buffer + 9, "%d", &client_pid);
            printf("\n[Main Thread -- %09lu]: Received command 'REGISTER' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, client_pid);

            char* cmd_copy = strdup(buffer);
            if (!cmd_copy) {
                perror("strdup failed");
                continue;
            }

            if (pthread_create(&thread, NULL, handle_client, cmd_copy) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long)thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long)thread);
            } else {
                perror("pthread_create failed");
                free(cmd_copy);
            }
        }
    }

    mq_close(mq);
    return 0;
}


/* else {
    pthread_t shell_thread;
    pthread_create(&shell_thread, NULL, handle_shell_command, strdup(command));
    pthread_detach(shell_thread);
}


void* handle_shell_command(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);

    pthread_t child_thread_id = pthread_self();

    // === Create a Pipe for Output ===
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        pthread_exit(NULL);
    }

    // === Fork a New Process ===
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        pthread_exit(NULL);
    }

    if (pid == 0) {  // Child Process
        close(pipefd[0]);  // Close read end
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout
        dup2(pipefd[1], STDERR_FILENO);  // Redirect stderr
        close(pipefd[1]);  // Close write end

        // === Log Process Creation ===
        printf("[Child Thread * %015lu]: Spawning a new child process (PID: %d) to execute command '%s'\n", 
                child_thread_id % 1000000000000000, getpid(), command);

        execlp("/bin/bash", "bash", "-c", command, NULL);
        perror("execlp failed");
        exit(1);
    } else {  // Parent Process
        close(pipefd[1]);  // Close write end

        // === Log Execution ===
        printf("[Child Thread * %015lu]: Command '%s' executed by child process (PID: %d)\n", 
                child_thread_id % 1000000000000000, command, pid);

        // === Wait for Process to Complete with Timeout ===
        sleep(3);
        int status;
        if (waitpid(pid, &status, WNOHANG) == 0) {
            kill(pid, SIGKILL);
            printf("[Child Thread * %015lu]: Command '%s' timed out and was killed (PID: %d)\n", 
                    child_thread_id % 1000000000000000, command, pid);
        }
        close(pipefd[0]);
    }

    pthread_exit(NULL);
}


 */