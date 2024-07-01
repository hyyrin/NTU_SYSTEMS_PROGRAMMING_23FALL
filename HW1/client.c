#include "hw1.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUFFER_SIZE 4096
typedef struct {
    char* ip; // server's ip
    unsigned short port; // server's port
    int conn_fd; // fd to talk with server
    char buf[BUFFER_SIZE]; // data sent by/to server
    size_t buf_len; // bytes used by buf
} client;
client cli;
static void init_client(char** argv);

int main(int argc, char** argv){
    
    // Parse args.
    if(argc!=3){
        ERR_EXIT("usage: [ip] [port]");
    }
    // Handling connection
    init_client(argv);
    fprintf(stderr, "connect to %s %d\n", cli.ip, cli.port);
    printf("==============================\n");
    printf("Welcome to CSIE Bulletin board\n");
    printf("==============================\n");
    char message[4096];
    strcpy(message, "pull");
    write(cli.conn_fd, message, strlen(message));
    char buf[4096];
    int res;
    res = read(cli.conn_fd, buf, sizeof(buf));
    buf[res] = '\0';
    int cnt = 0;
    for (int i = 1; i < strlen(buf); i++)
        if (buf[i] == '*')
            cnt++;
    int dir = 0;
    int flag = 0;
    if (strlen(buf) > 0 && cnt > 0){
        printf("FROM: ");
        flag = 1;
    }
    for (int i = 1; i < strlen(buf) && cnt > 0; i++){
        if (buf[i] != '*'){
            printf("%c", buf[i]);
        }
        if (buf[i] == '*' && dir % 2 == 0){
            printf("\n");
            printf("CONTENT:\n");
            dir++;
            cnt--;
        }
        else if (buf[i] == '*' && dir % 2 == 1 && cnt > 1){
            printf("\n");
            printf("FROM: ");
            dir++;
            cnt--;
        }
    }
    if (flag)
        printf("\n");
    printf("==============================\n");
    while(1){
        // TODO: handle user's input
        char ipt[4096];
        printf("Please enter your command (post/pull/exit): ");
        fgets(ipt, sizeof(ipt), stdin);
        if (strcmp(ipt, "pull\n") == 0){
            char message[1024];
            strcpy(message, "pull");
            write(cli.conn_fd, message, strlen(message));
            printf("==============================\n");
            char buf[4096];
            int res;
            res = read(cli.conn_fd, buf, sizeof(buf));
            buf[res] = '\0';
            int cnt = 0;
            for (int i = 1; i < strlen(buf); i++)
                if (buf[i] == '*')
                    cnt++;
            int dir = 0;
            int flag = 0;
            if (strlen(buf) > 0 && cnt > 0){
                printf("FROM: ");
                flag = 1;
            }
            for (int i = 1; i < strlen(buf) && cnt > 0; i++){
                if (buf[i] != '*')
                    printf("%c", buf[i]);
                if (buf[i] == '*' && dir % 2 == 0){
                    printf("\n");
                    printf("CONTENT:\n");
                    dir++;
                    cnt--;
                }
                else if (buf[i] == '*' && dir % 2 == 1 && cnt > 1){
                    printf("\n");
                    printf("FROM: ");
                    dir++;
                    cnt--;
                }
            }
            if (flag)
                printf("\n");
            printf("==============================\n");
        }
        else if (strcmp(ipt, "post\n") == 0){
            char message[4096];
            strcpy(message, "check");
            write(cli.conn_fd, message, strlen(message));
            char buf[4096];
            int res;
            res = read(cli.conn_fd, buf, sizeof(buf));
            buf[res] = '\0';
            if (strcmp(buf, "yes") == 0)
                printf("[Error] Maximum posting limit exceeded\n");
            else if (strcmp(buf, "no") == 0){
                char tot[4096];
                printf("FROM: ");
                char from[4096];
                char content[4096];
                fgets(from, sizeof(from), stdin);
                strcpy(tot, from);
                printf("CONTENT:\n");
                fgets(content, sizeof(content), stdin);
                strcat(tot, content);
                write(cli.conn_fd, tot, strlen(tot));
            }
        }
        else if (strcmp(ipt, "exit\n") == 0){
            char cmd[4096];
            strcpy(cmd, "exit");
            write(cli.conn_fd, cmd, strlen(cmd));
            break;
        }
    }
 
}

static void init_client(char** argv){
    
    cli.ip = argv[1];

    if(atoi(argv[2])==0 || atoi(argv[2])>65536){
        ERR_EXIT("Invalid port");
    }
    cli.port=(unsigned short)atoi(argv[2]);

    struct sockaddr_in servaddr;
    cli.conn_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(cli.conn_fd<0){
        ERR_EXIT("socket");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(cli.port);

    if(inet_pton(AF_INET, cli.ip, &servaddr.sin_addr)<=0){
        ERR_EXIT("Invalid IP");
    }

    if(connect(cli.conn_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){
        ERR_EXIT("connect");
    }

    return;
}

