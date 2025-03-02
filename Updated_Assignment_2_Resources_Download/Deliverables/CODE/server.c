#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>

#define SERVER_QUEUE "/server_queue"
#define CLIENT_SHUTDOWN_QUEUE_PREFIX "/client_shutdown_"
#define MAX_MSG_SIZE 1024
#define MAX_CLIENTS 10

// Updated Client structure with a visible flag.
typedef struct {
    int pid;
    char queue_name[50];
    int visible;  // 1 = visible; 0 = hidden.
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

// Handler for LIST command: "LIST <pid>"
// This function builds the response and sends it.
void* handle_list(void* arg) {
    int client_pid = *((int*)arg);
    free(arg);
    char response[MAX_MSG_SIZE] = "Connected Clients:\n";
    char response_queue[50] = "";
    int found = 0;
    int visible_clients = 0;  // Add a counter for visible clients

    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == client_pid) {
            strcpy(response_queue, clients[i].queue_name);
            found = 1;
        }
        if (clients[i].visible) {
            visible_clients++;  // Increment counter for each visible client
            char client_info[50];
            snprintf(client_info, sizeof(client_info), "Client %d --> (PID %d)\n", i + 1, clients[i].pid);
            strcat(response, client_info);
        }
    }
    
    // If no visible clients were found, update the response
    if (visible_clients == 0 && client_count > 0) {
        strcpy(response, "Connected Clients:\nAll clients are hidden.\n");
    }
    
    pthread_mutex_unlock(&lock);

    if (!found) {
        printf("[Main Thread -- %09lu]: Could not find client with PID %d in client list.\n",
               pthread_self() % 1000000000, client_pid);
        pthread_exit(NULL);
    }

    printf("[Server] Sending LIST response to client queue: %s\n", response_queue);
    mqd_t client_mq;
    for (int i = 0; i < 5; i++) {
        client_mq = mq_open(response_queue, O_WRONLY);
        if (client_mq != (mqd_t)-1) {
            if (mq_send(client_mq, response, strlen(response) + 1, 0) != -1) {
                mq_close(client_mq);
                pthread_exit(NULL);
            }
            mq_close(client_mq);
        }
        usleep(100000);
    }
    perror("[Server] Failed to send LIST response after retries");
    pthread_exit(NULL);
}

// Handler for HIDE command: "HIDE <pid>"
void* handle_hide(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);
    int pid;
    if (sscanf(command, "HIDE %d", &pid) != 1) {
        pthread_exit(NULL);
    }
    int found = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == pid) {
            found = 1;
            if (clients[i].visible == 0) {
                pthread_mutex_unlock(&lock);
                send_response(clients[i].queue_name, "You Are Already Hidden...");
            } else {
                clients[i].visible = 0;
                pthread_mutex_unlock(&lock);
                send_response(clients[i].queue_name, "You Are Now Hidden...");
            }
            break;
        }
    }
    if (!found) {
        pthread_mutex_unlock(&lock);
        printf("[Main Thread -- %09lu]: HIDE: Client with PID %d not found.\n",
               pthread_self() % 1000000000, pid);
    }
    pthread_exit(NULL);
}

// Handler for UNHIDE command: "UNHIDE <pid>"
void* handle_unhide(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);
    int pid;
    if (sscanf(command, "UNHIDE %d", &pid) != 1) {
        pthread_exit(NULL);
    }
    int found = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == pid) {
            found = 1;
            if (clients[i].visible == 1) {
                pthread_mutex_unlock(&lock);
                send_response(clients[i].queue_name, "You Are Not Hidden At All...");
            } else {
                clients[i].visible = 1;
                pthread_mutex_unlock(&lock);
                send_response(clients[i].queue_name, "You Are Now Visible Again...");
            }
            break;
        }
    }
    if (!found) {
        pthread_mutex_unlock(&lock);
        printf("[Main Thread -- %09lu]: UNHIDE: Client with PID %d not found.\n",
               pthread_self() % 1000000000, pid);
    }
    pthread_exit(NULL);
}

// Handler for uppercase EXIT command: "EXIT <pid>"
// Disconnects the client.
void* handle_exit(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);
    int pid;
    if (sscanf(command, "EXIT %d", &pid) != 1) {
        pthread_exit(NULL);
    }
    
    // Print the cleanup message
    printf("[Child Thread * %015lu]: Cleaning up client (PID %d) resources...\n",
           pthread_self() % 1000000000000000, pid);
    
    int index = -1;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == pid) {
            index = i;
            break;
        }
    }
    if (index != -1) {
        char queue_name[50];
        strcpy(queue_name, clients[index].queue_name);
        for (int j = index; j < client_count - 1; j++) {
            clients[j] = clients[j+1];
        }
        client_count--;
        pthread_mutex_unlock(&lock);
        send_response(queue_name, "Client disconnected.");
    } else {
        pthread_mutex_unlock(&lock);
    }
    pthread_exit(NULL);
}

// Handler for lowercase exit command: "exit <pid>"
// This command is ignored.
void* handle_lower_exit(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);
    int pid;
    if (sscanf(command, "exit %d", &pid) != 1) {
        pthread_exit(NULL);
    }
    char queue_name[50] = "";
    int found = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].pid == pid) {
            strcpy(queue_name, clients[i].queue_name);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    if (found) {
        send_response(queue_name, "Ignored 'exit' command as it may Exit the Shell Session...");
    }
    pthread_exit(NULL);
}

// Handler for SHELL command: "SHELL <pid> <command string>"
// Executes the shell command with a 3-second timeout.
void* handle_shell_command(void* arg) {
    char *msg = (char*) arg;
    int pid;
    char shell_cmd[MAX_MSG_SIZE];
    if (sscanf(msg, "SHELL %d %[^\n]", &pid, shell_cmd) != 2) {
        free(msg);
        pthread_exit(NULL);
    }
    free(msg);
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        pthread_exit(NULL);
    }
    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        pthread_exit(NULL);
    }
    if (child_pid == 0) {  // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("/bin/bash", "bash", "-c", shell_cmd, NULL);
        perror("execlp failed");
        exit(1);
    } else {
        close(pipefd[1]);
        int status;
        int waited = 0;
        while (waited < 30) {  // 30*100ms = 3 seconds
            pid_t result = waitpid(child_pid, &status, WNOHANG);
            if (result == 0) {
                usleep(100000);
                waited++;
            } else {
                break;
            }
        }
        if (waited >= 30) {
            kill(child_pid, SIGKILL);
            waitpid(child_pid, &status, 0);
            char timeout_msg[] = "Command Timeout...";
            char client_queue[50] = "";
            int found = 0;
            pthread_mutex_lock(&lock);
            for (int i = 0; i < client_count; i++) {
                if (clients[i].pid == pid) {
                    strcpy(client_queue, clients[i].queue_name);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);
            if (found)
                send_response(client_queue, timeout_msg);
            pthread_exit(NULL);
        } else {
            char output[MAX_MSG_SIZE];
            int n = read(pipefd[0], output, MAX_MSG_SIZE - 1);
            if (n < 0) n = 0;
            output[n] = '\0';
            close(pipefd[0]);
            char client_queue[50] = "";
            int found = 0;
            pthread_mutex_lock(&lock);
            for (int i = 0; i < client_count; i++) {
                if (clients[i].pid == pid) {
                    strcpy(client_queue, clients[i].queue_name);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);
            if (found) {
                if (strlen(output) == 0)
                    send_response(client_queue, "Command executed with no output.");
                else
                    send_response(client_queue, output);
            }
            pthread_exit(NULL);
        }
    }
}

// Handler for client registration: "REGISTER <pid> <queue_name>"
void* handle_client(void* arg) {
    char command[MAX_MSG_SIZE];
    strcpy(command, (char*)arg);
    free(arg);
    pthread_t child_thread_id = pthread_self();
    char queue_name[50];
    int pid = -1;
    if (strncmp(command, "REGISTER ", 9) == 0) {
        if (sscanf(command + 9, "%d %s", &pid, queue_name) != 2) {
            printf("[Main Thread -- %09lu]: Invalid REGISTER command format.\n", pthread_self() % 1000000000);
            pthread_exit(NULL);
        }
        pthread_mutex_lock(&lock);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].pid = pid;
            strcpy(clients[client_count].queue_name, queue_name);
            clients[client_count].visible = 1;  // Visible by default.
            client_count++;
        }
        pthread_mutex_unlock(&lock);
        printf("\n[Child Thread * %015lu]: Registered client (PID: %d) to the client list. Total Clients ---> [%d]\n",
               child_thread_id % 1000000000000000, pid, client_count);
        printf("[Child Thread * %015lu]: Registered the Shutdown broadcast message queue '%s'\n",
               child_thread_id % 1000000000000000, queue_name);
    }
    pthread_exit(NULL);
}


// Signal handler for cleanup and SHUTDOWN broadcast
void cleanup_server(int signum) {
    unsigned long main_thread_id = pthread_self() % 1000000000;

    printf("\n----------------------------------------------------------------------\n");
    printf("[Main Thread -- %09lu]: Signal %d received...\n", main_thread_id, signum);
    printf("[Main Thread -- %09lu]: Gracefully exiting...\n", main_thread_id);
    printf("[Main Thread -- %09lu]: Cleaning up server and client resources...\n", main_thread_id);
    printf("[Main Thread -- %09lu]: Broadcasting 'SHUTDOWN' message to all the clients...\n", main_thread_id);

    // Send SHUTDOWN message to all clients without extra logs
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        char shutdown_queue[50];
        snprintf(shutdown_queue, sizeof(shutdown_queue), "/client_shutdown_%d", clients[i].pid);
        mqd_t mq = mq_open(shutdown_queue, O_WRONLY);
        if (mq != (mqd_t)-1) {
            char shutdown_msg[] = "SHUTDOWN";
            mq_send(mq, shutdown_msg, strlen(shutdown_msg) + 1, 0);
            mq_close(mq);
        }
    }
    pthread_mutex_unlock(&lock);

    // Cleanup
    mq_unlink(SERVER_QUEUE);
    exit(0);
}



// Main server function
int main() {
    mqd_t mq;
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};

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
        if (bytes_read < 0) {
            perror("mq_receive failed");
            continue;
        }
        buffer[bytes_read] = '\0';
        pthread_t thread;

        if (strncmp(buffer, "REGISTER ", 9) == 0) {
            int client_pid;
            sscanf(buffer + 9, "%d", &client_pid);
            printf("\n[Main Thread -- %09lu]: Received command 'REGISTER' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, client_pid);
            if (pthread_create(&thread, NULL, handle_client, strdup(buffer)) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
            }
        }
        else if (strncmp(buffer, "LIST", 4) == 0) {
            int client_pid;
            if (sscanf(buffer, "LIST %d", &client_pid) != 1) {
                printf("[Main Thread -- %09lu]: LIST command received without client PID. Ignoring.\n",
                       pthread_self() % 1000000000);
                continue;
            }
            int* pid_ptr = malloc(sizeof(int));
            if (!pid_ptr) continue;
            *pid_ptr = client_pid;
            printf("\n[Main Thread -- %09lu]: Received command 'LIST' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, client_pid);
            if (pthread_create(&thread, NULL, handle_list, pid_ptr) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
                free(pid_ptr);
            }
        }
        else if (strncmp(buffer, "HIDE", 4) == 0) {
            printf("\n[Main Thread -- %09lu]: Received command 'HIDE' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, atoi(strchr(buffer, ' ') + 1));
            if (pthread_create(&thread, NULL, handle_hide, strdup(buffer)) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
            }
        }
        else if (strncmp(buffer, "UNHIDE", 6) == 0) {
            printf("\n[Main Thread -- %09lu]: Received command 'UNHIDE' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, atoi(strchr(buffer, ' ') + 1));
            if (pthread_create(&thread, NULL, handle_unhide, strdup(buffer)) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
            }
        }
        else if (strncmp(buffer, "EXIT", 4) == 0) {
            printf("\n[Main Thread -- %09lu]: Received command 'EXIT' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, atoi(strchr(buffer, ' ') + 1));
            if (pthread_create(&thread, NULL, handle_exit, strdup(buffer)) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
            }
        }
        else if (strncmp(buffer, "exit", 4) == 0) {
            printf("\n[Main Thread -- %09lu]: Received command 'exit' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, atoi(strchr(buffer, ' ') + 1));
            if (pthread_create(&thread, NULL, handle_lower_exit, strdup(buffer)) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
            }
        }
        else if (strncmp(buffer, "SHELL ", 6) == 0) {
            printf("\n[Main Thread -- %09lu]: Received command 'SHELL' from the client (PID %d). About to create a child thread.\n",
                   pthread_self() % 1000000000, atoi(strchr(buffer, ' ') + 1));
            if (pthread_create(&thread, NULL, handle_shell_command, strdup(buffer)) == 0) {
                printf("[Main Thread -- %09lu]: Successfully created the child thread [%015lu]\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
                pthread_detach(thread);
                printf("[Main Thread -- %09lu]: The child thread [%015lu] successfully exited\n",
                       pthread_self() % 1000000000, (unsigned long) thread);
            } else {
                perror("pthread_create failed");
            }
        }
        else {
            printf("[Server] Unknown command received: %s\n", buffer);
        }
    }

    mq_close(mq);

    return 0;
}
