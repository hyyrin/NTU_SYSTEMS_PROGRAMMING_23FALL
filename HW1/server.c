#include "hw1.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUFFER_SIZE 4096

record *all;

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[BUFFER_SIZE];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list


// initailize a server, exit for error
static void init_server(unsigned short port);

// initailize a request instance
static void init_request(request* reqP);

// free resources used by a request instance
static void free_request(request* reqP);

int last = -1;
int locked[RECORD_NUM] = {0};
int is_locked(int fd, int start, int whence){
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = start;
    lock.l_len = sizeof(record);
    lock.l_whence = whence;
    int trylock =  fcntl(fd, F_GETLK, &lock);
    if(trylock==-1){
        fprintf(stderr, " errno = %d\n",errno);
        return -1;
    }
    return (lock.l_type == F_UNLCK) ? 0 : 1;
}
void dolock(int fd, int start, int whence, int locktype){
    struct flock lock;
    lock.l_type = locktype;
    lock.l_start = start;
    lock.l_len = sizeof(record);
    lock.l_whence = whence;
    int trylock =  fcntl(fd, F_SETLK, &lock);
    if(trylock==-1){
        fprintf(stderr, "errno = %d\n",errno);
        perror("Error : ");
    }
}
int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        ERR_EXIT("usage: [port]");
        exit(1);
    }
    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[BUFFER_SIZE];
    int buf_len;
    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    int DB = open(RECORD_PATH, O_RDWR| O_CREAT, 0644);
    //int DB = open("./BulletinBoard", O_RDWR, 0644);
    //int DB = openat( AT_FDCWD, RECORD_PATH, O_RDWR);
    all = malloc(sizeof(record) * RECORD_NUM);
    struct timeval tw;
       tw.tv_sec = 0;
       tw.tv_usec = 50;
    fd_set master;
    if (DB < 0)
        printf("does not exist\n");
    while (1) {
        // TODO: Add IO multiplexing
        FD_ZERO(&master);
        FD_SET(svr.listen_fd, &master);
        read(DB, all, sizeof(record) * RECORD_NUM);
        for (int i = 0; i < maxfd; i++) {
            if (requestP[i].conn_fd > 0)
                FD_SET(requestP[i].conn_fd, &master);
        }
        int rtv = select(maxfd + 1, &master, NULL, NULL, NULL);
         //Check new connection
        if (FD_ISSET(svr.listen_fd, &master)){
            clilen = sizeof(cliaddr);
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept");
            }
            requestP[conn_fd].conn_fd = conn_fd;
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
            int num_of_locked = 0;
            for (int i = 0; i < RECORD_NUM; i++)
                if (is_locked(DB, i * sizeof(record), SEEK_SET) || locked[i])
                    num_of_locked++;
            if (num_of_locked > 0){
                printf("[Warning] Try to access locked post - %d\n", num_of_locked);
                fflush(stdout);
            }
        }
        // TODO: handle requests from clients
        for (int con_fd = 0; con_fd < maxfd; con_fd++){
            if (FD_ISSET(requestP[con_fd].conn_fd, &master) && con_fd != svr.listen_fd){
                
                int ret;
                char buf[4096];
                char message[4096];
                ret = read(requestP[con_fd].conn_fd, buf, sizeof(buf));
                if (ret == 0){
                    printf("disconnected\n");
                    close(requestP[con_fd].conn_fd);
                    free(&requestP[con_fd]);
                }
                else{
                    buf[ret] = '\0';
                    if (strcmp(buf, "pull") == 0)
                        strcpy(message, "pull");
                    else if (strcmp(buf, "check") == 0)
                        strcpy(message, "check");
                    else if (strcmp(buf, "exit") == 0)
                        strcpy(message, "exit");
                    else
                        strcpy(message, "post");
                }
                if (strcmp(message, "pull") == 0){
                    int num_of_locked = 0;
                    for (int i = 0; i < RECORD_NUM; i++)
                        if (is_locked(DB, i * sizeof(record), SEEK_SET) || locked[i])
                            num_of_locked++;
                    if (num_of_locked > 0){
                        printf("[Warning] Try to access locked post - %d\n", num_of_locked);
                        fflush(stdout);
                    }
                    char tmp[4096];
                    strcpy(tmp, "'\0");
                    for (int i = 0; i < RECORD_NUM; i++){
                        if (!is_locked(DB, i * sizeof(record), SEEK_SET) && all[i].From[0] != '\0' && all[i].Content[0] != '\0' && !locked[i]){
                        //if (!is_locked(DB, i * sizeof(record), SEEK_SET) && !locked[i]){
                            strcat(tmp, all[i].From);
                            strcat(tmp, "*");
                            strcat(tmp, all[i].Content);
                            strcat(tmp, "*");
                        }
                    }
                    write(requestP[con_fd].conn_fd, tmp, strlen(tmp));
                }
                else if (strcmp(message, "check") == 0){
                    int num_of_locked = 0;
                    for (int i = 0; i < RECORD_NUM; i++)
                        if (is_locked(DB, i * sizeof(record), SEEK_SET) || locked[i])
                            num_of_locked++;
                    char tmp[4096];
                    if (num_of_locked == RECORD_NUM)
                        strcpy(tmp, "yes");
                    else
                        strcpy(tmp, "no");
                    write(requestP[con_fd].conn_fd, tmp, strlen(tmp));
                }
                else if (strcmp(message, "post") == 0){
                    int pos = 0;
                    int flag = 0;
                    for (int i = last + 1; i < last + RECORD_NUM + 1; i++){
                        int j = i % RECORD_NUM;
                        //if (!is_locked(DB, j * sizeof(record), SEEK_SET) && all[j].From[0] == '\0' && all[j].Content[0] == '\0' && !locked[j]){
                        if (!is_locked(DB, j * sizeof(record), SEEK_SET) && !locked[j]){
                            dolock(DB, j * sizeof(record), SEEK_SET, F_WRLCK);
                            locked[j] = 1;
                            //last = i + 1;
                            last = i;
                            flag = 1;
                            pos = j;
                            break;
                        }
                    }
                    if (!flag){
                        for (int i = last + 1; i < last + RECORD_NUM + 1; i++){
                            int j = i % RECORD_NUM;
                            //if (!is_locked(DB, j * sizeof(record), SEEK_SET) && !locked[j]){
                            if (!is_locked(DB, j * sizeof(record), SEEK_SET) && !locked[j]){
                                dolock(DB, j * sizeof(record), SEEK_SET, F_WRLCK);
                                locked[j] = 1;
                                last = i;
                                pos = j;
                                break;
                            }
                        }
                    }
                    lseek(DB, pos * sizeof(record), SEEK_SET);
                    int res;
                    record tmp;
                    int j;
                    for (int i = 0; i < strlen(buf); i++){
                        if (buf[i] != '\n')
                            tmp.From[i] = buf[i];
                        else{
                            tmp.From[i] = '\0';
                            j = i;
                            break;
                        }
                    }
                    
                    for (int i = j + 1, k = 0; i < strlen(buf); i++, k++){
                        if (buf[i] != '\n')
                            tmp.Content[k] = buf[i];
                        else{
                            tmp.Content[k] = '\0';
                            break;
                        }
                    }
                    ssize_t byte_write = write(DB, &tmp, sizeof(record));
                    lseek(DB, 0, SEEK_SET);
                    dolock(DB, pos * sizeof(record), SEEK_SET, F_UNLCK);
                    locked[pos] = 0;
                    printf("[Log] Receive post from %s\n", tmp.From);
                    fflush(stdout);
                }
                else if (strcmp(message, "exit") == 0){
                    close(requestP[con_fd].conn_fd);
                    free_request(&requestP[con_fd]);
                }
            }
        }
    }
    close(DB);
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}

