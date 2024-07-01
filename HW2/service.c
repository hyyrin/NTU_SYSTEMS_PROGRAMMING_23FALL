#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "util.h"

#define ERR_EXIT(s) perror(s), exit(errno);
#define PARENT_READ_FD 3
#define PARENT_WRITE_FD 4
#define MAX_CHILDREN 8
#define MAX_FIFO_NAME_LEN 64
#define MAX_SERVICE_NAME_LEN 16
#define MAX_CMD_LEN 128
#include <sys/types.h>
/*typedef struct service{
    pid_t pid;
    int read_fd;
    int write_fd;
    char name[MAX_SERVICE_NAME_LEN];
    int have_killed;
} service;*/

#define MAX_COMMAND_LEN 50
typedef struct Node{
    service *node;
    struct Node *next;
    int child_num;
    //int have_killed;
}Node;
service *manager = NULL;
static unsigned long secret;
static char service_name[MAX_SERVICE_NAME_LEN];

static inline bool is_manager() {
    return strcmp(service_name, "Manager") == 0;
}

void print_not_exist(char *service_name) {
    printf("%s doesn't exist\n", service_name);
}

void print_receive_command(char *service_name, char *cmd) {
    printf("%s has received %s\n", service_name, cmd);
}

void print_spawn(char *parent_name, char *child_name) {
    printf("%s has spawned a new service %s\n", parent_name, child_name);
}

void print_kill(char *target_name, int decendents_num) {
    printf("%s and %d child services are killed\n", target_name, decendents_num);
}

void print_acquire_secret(char *service_a, char *service_b, unsigned long secret) {
    printf("%s has acquired a new secret from %s, value: %lu\n", service_a, service_b, secret);
}

void print_exchange(char *service_a, char *service_b) {
    printf("%s and %s have exchanged their secrets\n", service_a, service_b);
}
void create_service(char *child_name, Node *target) {
    Node *record = target;
    service *new_service = (service *)malloc(sizeof(service));
    new_service->have_killed = 0;
    Node *new_child = (Node *)malloc(sizeof(Node));
    new_child->node = new_service;
    new_child->next = NULL;
    while (target->next != NULL)
        target = target->next;
    target->next = new_child;
    strcpy(new_service->name, child_name);
    record->child_num++;
    int fd1[2];
    int fd2[2];
    //O_CLOEXEC
    if (pipe(fd1) == -1 || pipe(fd2) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    /*int flag1_read = fcntl(fd1[0], F_GETFD);
    int flag1_write = fcntl(fd1[1], F_GETFD);
    int flag2_read = fcntl(fd2[0], F_GETFD);
    int flag2_write = fcntl(fd2[1], F_GETFD);
    flag1_read |= FD_CLOEXEC;
    flag1_write |= FD_CLOEXEC;
    flag2_read |= FD_CLOEXEC;
    flag2_write |= FD_CLOEXEC;
    fcntl(fd1[0], F_SETFD, flag1_read);
    fcntl(fd1[1], F_SETFD, flag1_write);
    fcntl(fd2[0], F_SETFD, flag2_read);
    fcntl(fd2[1], F_SETFD, flag2_write);*/
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        close(fd1[1]);
        close(fd2[0]);
        dup2(fd1[0], 3);
        dup2(fd2[1], 4);
        execlp("./service", "./service", child_name, (char*)NULL);
        exit(EXIT_SUCCESS);
    }
    else {
        close(fd2[1]);
        close(fd1[0]);
        new_service->pid = pid;
        new_service->read_fd = fd2[0];
        new_service->write_fd = fd1[1];
        char need[512];
        strcpy(need, service_name);
        write(new_child->node->write_fd, need, strlen(need) + 1);
    }
}

int main(int argc, char *argv[]) {
    pid_t pid = getpid();

    if (argc != 2) {
        fprintf(stderr, "Usage: ./service [service_name]\n");
        return 0;
    }

    srand(pid);
    secret = rand();
    setvbuf(stdout, NULL, _IONBF, 0);

    strncpy(service_name, argv[1], MAX_SERVICE_NAME_LEN);
    printf("%s has been spawned, pid: %d, secret: %lu\n", service_name, pid, secret);
    Node *root = (Node *)malloc(sizeof(Node));
    service *a = (service *)malloc(sizeof(service));
    strcpy(a->name, service_name);
    a->have_killed = 0;
    a->pid = pid;
    root->node = a;
    root->child_num = 0;
    root->node = manager;
    root->next = NULL;
    if (is_manager()){
        while (1) {
            char command[MAX_COMMAND_LEN];
            scanf("%s", command);
            if (strcmp(command, "spawn") == 0) {
                char parent_name[20];
                char child_name[20];
                scanf("%s", parent_name);
                scanf("%s", child_name);
                char message[512];
                sprintf(message, "spawn %s %s", parent_name, child_name);
                Node *tmp = root;
                if (strcmp("Manager", parent_name) == 0){
                    printf("%s has received %s\n", service_name, message);
                    create_service(child_name, tmp);
                }
                else{
                    printf("Manager has received %s\n", message);
                    tmp = root->next;
                    while (tmp != NULL){
                        if (tmp->node->have_killed){
                            tmp = tmp->next;
                            continue;
                        }
                        write(tmp->node->write_fd, message, strlen(message) + 1);
                        char buf[512], want[512];
                        read(tmp->node->read_fd, buf, sizeof(buf));
                        if (strcmp(buf, "yes") == 0)
                            break;
                        tmp = tmp->next;
                    }
                    if (tmp == NULL){
                        printf("%s doesn't exist\n", parent_name);
                    }
                }
            }
            else if (strcmp(command, "kill") == 0){
                char parent_name[32];
                scanf("%s", parent_name);
                char message[512];
                sprintf(message,"kill %s", parent_name);
                printf("Manager has received %s\n", message);
                Node *tmp = root->next;
                if (strcmp(parent_name, "Manager") == 0){
                    char m[16];
                    strcpy(m, "dead");
                    while (tmp != NULL){
                        if (tmp->node->have_killed){
                            tmp = tmp->next;
                            continue;
                        }
                        write(tmp->node->write_fd, m, strlen(m));
                        close(tmp->node->write_fd);
                        close(tmp->node->read_fd);
                        wait(NULL);
                        tmp = tmp->next;
                    }
                    printf("%s and %d child services are killed\n", parent_name, root->child_num);
                    break;
                }
                char find[512];
                sprintf(find, "find %s", parent_name);
                tmp = root->next;
                char want[512];
                strcpy(want, "haha");
                while (tmp != NULL){
                    if (tmp->node->have_killed){
                        tmp = tmp->next;
                        continue;
                    }
                    write(tmp->node->write_fd, find, strlen(find) + 1);
                    char buf[512];
                    read(tmp->node->read_fd, buf, sizeof(buf));
                    buf[3] = '\0';
                    if (strcmp(buf, "yes") == 0){
                        strcpy(want, buf);
                        break;
                    }
                    tmp = tmp->next;
                }
                if (tmp == NULL && strcmp(want, "yes") != 0){
                    printf("%s doesn't exist\n", parent_name);
                    continue;
                }
                tmp = root->next;
                while (tmp != NULL){
                    if (tmp->node->have_killed){
                        tmp = tmp->next;
                        continue;
                    }
                    write(tmp->node->write_fd, message, strlen(message) + 1);
                    char buf[512], want[512];
                    read(tmp->node->read_fd, buf, sizeof(buf));
                    if (strncmp(buf, "yes", 3) == 0){
                        if (buf[3] == 'n'){
                            tmp->node->have_killed = 1;
                            close(tmp->node->read_fd);
                            close(tmp->node->write_fd);
                            //wait(NULL); //un
                        }
                        printf("%s and %d child services are killed\n", parent_name, tmp->child_num);
                        break;
                    }
                    tmp = tmp->next;
                }
            }
            else if (strcmp(command, "exchange") == 0) {
                char parent_name[20];
                char child_name[20];
                scanf("%s", parent_name);
                scanf("%s", child_name);
                char message[512];
                sprintf(message, "exchange %s %s", parent_name, child_name);
                Node *tmp = root;
                if (strcmp("Manager", parent_name) == 0){
                    printf("%s has received exchange %s %s\n", service_name, parent_name, child_name);
                    tmp = root->next;
                    while (tmp != NULL){
                        if (tmp->node->have_killed){
                            tmp = tmp->next;
                            continue;
                        }
                        write(tmp->node->write_fd, message, strlen(message) + 1);
                        char buf[512], want[512];
                        read(tmp->node->read_fd, buf, sizeof(buf));
                        buf[3] = '\0';
                        if (strncmp(buf, "yes", 3) == 0)
                            break;
                        tmp = tmp->next;
                    }
                }
                else{
                    printf("Manager has received exchange %s %s\n", parent_name, child_name);
                    tmp = root->next;
                    while (tmp != NULL){
                        if (tmp->node->have_killed){
                            tmp = tmp->next;
                            continue;
                        }
                        write(tmp->node->write_fd, message, strlen(message) + 1);
                        char buf[512], want[512];
                        read(tmp->node->read_fd, buf, sizeof(buf));
                        buf[3] = '\0';
                        if (strncmp(buf, "yes", 3) == 0)
                            break;
                        tmp = tmp->next;
                    }
                }
                printf("%s and %s have exchanged their secrets\n", parent_name, child_name);
            }
        }
    }
    else{
        char input[512];
        read(3, input, sizeof(input));
        printf("%s has spawned a new service %s\n", input, service_name);
        while (1){
            char buf[512];
            int rtv = read(3, buf, sizeof(buf));
            if (rtv <= 0)
                continue;
            if (buf[0] == 's'){
                printf("%s has received %s\n", service_name, buf);
                char want[512];
                int cnt_space = 0;
                int j = 0;
                for (int i = 0; i < strlen(buf); i++){
                    if (cnt_space == 1 && buf[i] != ' '){
                        want[j] = buf[i];
                        j++;
                    }
                    if (buf[i] == ' ')
                        cnt_space++;
                }
                want[j] = '\0';// parent_name
                char another[512];
                cnt_space = 0;
                j = 0;
                for (int i = 0; i < strlen(buf); i++){
                    if (cnt_space == 2 && buf[i] != ' '){
                        another[j] = buf[i];
                        j++;
                    }
                    if (buf[i] == ' ')
                        cnt_space++;
                }
                another[j] = '\0'; // child_name
                Node *tmp = root;
                char message[8];
                strcpy(message, "haha");
                char str[8];
                strcpy(str, "haha");
                if (strcmp(service_name, want) == 0){
                    create_service(another, tmp);
                    strcpy(message, "yes");
                    write(4, message, strlen(message));
                    continue;
                }
                tmp = root->next;
                char temp[8];
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                while (tmp != NULL){
                    if (tmp->node->have_killed){
                        tmp = tmp->next;
                        continue;
                    }
                    write(tmp->node->write_fd, buf, strlen(buf) + 1);
                    read(tmp->node->read_fd, str, sizeof(str));
                    str[3] = '\0';
                    if (strcmp(str, "yes") == 0)
                        break;
                    tmp = tmp->next;
                }
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                if (strcmp(str, "yes") == 0)
                    write(4, str, strlen(str));
                else if (strcmp(str, "not") == 0)
                    write(4, str, strlen(str));
            }
            else if (buf[0] == 'k'){
                char want[512];
                int flag = 0;
                int cnt_space = 0;
                int j = 0;
                for (int i = 0; i < strlen(buf); i++){
                    if (cnt_space == 1 && buf[i] != ' '){
                        want[j] = buf[i];
                        j++;
                    }
                    if (buf[i] == ' ')
                        cnt_space++;
                }
                want[j] = '\0';// parent_name
                Node *tmp = root;
                char message[8];
                strcpy(message, "haha");
                char str[8];
                strcpy(str, "haha");
                if (strcmp(service_name, want) == 0){
                    tmp = root->next;
                    char temp[8];
                    strcpy(temp, "dead");
                    while (tmp != NULL){
                        write(tmp->node->write_fd, temp, strlen(temp));
                        close(tmp->node->write_fd);
                        close(tmp->node->read_fd);
                        wait(NULL);
                        tmp = tmp->next;
                    }
                    strcpy(message, "yesn");
                    write(4, message, strlen(message));
                    break;
                }
                tmp = root->next;
                char temp[8];
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                while (tmp != NULL){
                    if (tmp->node->have_killed){
                        tmp = tmp->next;
                        continue;
                    }
                    write(tmp->node->write_fd, buf, strlen(buf) + 1);
                    read(tmp->node->read_fd, str, sizeof(str));
                    if (strncmp(str, "yes", 3) == 0){
                        str[3] = '\0';
                        tmp->node->have_killed = 1;
                        close(tmp->node->write_fd);
                        close(tmp->node->read_fd);
                        //wait(NULL); //un
                        break;
                    }
                    tmp = tmp->next;
                }
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                if (strcmp(str, "yes") == 0){
                    write(4, str, strlen(str));
                }
                else if (strcmp(str, "not") == 0)
                    write(4, str, strlen(str));
            }
            else if (buf[0] == 'd'){
                Node *tmp = root->next;
                char temp[8];
                strcpy(temp, "dead");
                while (tmp != NULL){
                    write(tmp->node->write_fd, temp, strlen(temp));
                    close(tmp->node->write_fd);
                    close(tmp->node->read_fd);
                    wait(NULL);
                    tmp = tmp->next;
                }
                break;
            }
            else if (buf[0] == 'f'){
                char want[512];
                int cnt_space = 0;
                int j = 0;
                for (int i = 0; i < strlen(buf); i++){
                    if (cnt_space == 1 && buf[i] != ' '){
                        want[j] = buf[i];
                        j++;
                    }
                    if (buf[i] == ' ')
                        cnt_space++;
                }
                want[j] = '\0';// parent_name
                printf("%s has received kill %s\n", service_name, want);
                Node *tmp = root;
                char message[8];
                strcpy(message, "haha");
                char str[8];
                strcpy(str, "haha");
                if (strcmp(service_name, want) == 0){
                    strcpy(message, "yes");
                    write(4, message, strlen(message));
                    continue;
                }
                tmp = root->next;
                char temp[8];
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                while (tmp != NULL){
                    if (tmp->node->have_killed){
                        tmp = tmp->next;
                        continue;
                    }
                    write(tmp->node->write_fd, buf, strlen(buf) + 1);
                    read(tmp->node->read_fd, str, sizeof(str));
                    str[3] = '\0';
                    if (strcmp(str, "yes") == 0)
                        break;
                    tmp = tmp->next;
                }
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                if (strcmp(str, "yes") == 0)
                    write(4, str, strlen(str));
                else if (strcmp(str, "not") == 0)
                    write(4, str, strlen(str));
            }
            else if (buf[0] == 'e'){
                printf("%s has received %s\n", service_name, buf);
                char want[512];
                int cnt_space = 0;
                int j = 0;
                for (int i = 0; i < strlen(buf); i++){
                    if (cnt_space == 1 && buf[i] != ' '){
                        want[j] = buf[i];
                        j++;
                    }
                    if (buf[i] == ' ')
                        cnt_space++;
                }
                want[j] = '\0';// parent_name
                char another[512];
                cnt_space = 0;
                j = 0;
                for (int i = 0; i < strlen(buf); i++){
                    if (cnt_space == 2 && buf[i] != ' '){
                        another[j] = buf[i];
                        j++;
                    }
                    if (buf[i] == ' ')
                        cnt_space++;
                }
                another[j] = '\0'; // child_name
                Node *tmp = root;
                char message[8];
                strcpy(message, "haha");
                char str[8];
                strcpy(str, "haha");
                if (strcmp(service_name, another) == 0){
                    strcpy(message, "yes");
                    write(4, message, strlen(message));
                    continue;
                }
                tmp = root->next;
                char temp[8];
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                while (tmp != NULL){
                    if (tmp->node->have_killed){
                        tmp = tmp->next;
                        continue;
                    }
                    write(tmp->node->write_fd, buf, strlen(buf) + 1);
                    read(tmp->node->read_fd, str, sizeof(str));
                    str[3] = '\0';
                    if (strncmp(str, "yes", 3) == 0)
                        break;
                    tmp = tmp->next;
                }
                if (tmp == NULL){
                    strcpy(temp, "not");
                    write(4, temp, strlen(temp));
                    continue;
                }
                if (strncmp(str, "yes", 3) == 0)
                    write(4, str, strlen(str));
                else if (strcmp(str, "not") == 0)
                    write(4, str, strlen(str));
            }
        }
    }
    
    return 0;
}

