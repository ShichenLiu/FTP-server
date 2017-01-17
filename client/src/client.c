#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <memory.h>
#include <stdio.h>

#define printf(...) fprintf(stderr,__VA_ARGS__)

char CUSER[100];
char CPASS[100];

typedef struct Status {
    int status;
    int gui;
    char* path;
    int port;
    char host[100];
    int sockfd;
    int fileport;
    int filefd;
    char filehost[100];
    char filename[100];
    int pasv;
}Status;

int isnum(char c) {
    return (c <= '9' || c >= '0') ? 1 : 0;
}

int iscmd(char* msg, char* cmd) {
    if (strlen(msg) < strlen(cmd)) return 0;
    return strstr(msg, cmd) == msg;
}

int parsecmd(char* input, Status* status) {
    FILE *f;
    char cmd[100];
    int info[10], connfd;
    struct sockaddr_in addr;
    if (iscmd(input, "PWD")) return 10;
    if (iscmd(input, "CWD")) return 13;
    strcpy(cmd, input + 5);
    cmd[strlen(cmd) - 2] = '\0';
    if (iscmd(input, "SYST")) return 6;
    if (iscmd(input, "TYPE")) return 7;
    if (iscmd(input, "PASV")) return 8;
    if (iscmd(input, "CDUP")) return 13;
    if (iscmd(input, "LIST")) {
        if (status->pasv == 1) { // PASV
            if (status->filefd != -1) return 14;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = status->fileport;
            if ((status->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            if (inet_pton(AF_INET, status->filehost, &addr.sin_addr) <= 0) {
                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            if (connect(status->filefd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            connfd = status->filefd;
        }
        return 14;
    }
    if (iscmd(input, "DELE")) return 15;
    if (iscmd(input, "RNFR")) return 18;
    if (iscmd(input, "RNTO")) return 19;    
    if (iscmd(input, "RMD")) return 15;
    if (iscmd(input, "MKD")) return 15;
    if (iscmd(input, "PORT")) {
        sscanf(input, "PORT %d,%d,%d,%d,%d,%d", &info[0], &info[1], &info[2], &info[3], &info[4], &info[5]);
        status->fileport = info[4] * 256 + info[5];
        sprintf(status->filehost, "%d.%d.%d.%d", info[0], info[1], info[2], info[3]);
        return 9;
    }
    if (iscmd(input, "STOR")) {
        if (status->pasv == 1) {
            if (status->filefd != -1) return 11;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = status->fileport;
            if ((status->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            if (inet_pton(AF_INET, status->filehost, &addr.sin_addr) <= 0) {
                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            if (connect(status->filefd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
        }
        f = fopen(cmd, "r");
        if (f == NULL) {
            printf("no such file: %s\n", cmd);
            return -1;
        }
        fclose(f);
        strcpy(status->filename, cmd);
        return 11;
    }
    if (iscmd(input, "RETR")) {
        if (status->pasv == 1) {
            if (status->filefd != -1) return 12;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = status->fileport;
            if ((status->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            if (inet_pton(AF_INET, status->filehost, &addr.sin_addr) <= 0) {
                printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
            if (connect(status->filefd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                return -1;
            }
        }
        strcpy(status->filename, cmd);
        return 12;
    }
    return -1;
}

void replyServer(Status* status, char* msg) {
    int index, i;
    struct sockaddr_in addr;
    char buff[4096];
    int p, len;
    FILE* f;
    int p1, p2, h1, h2, h3, h4;
    if (isnum(msg[0]) && isnum(msg[1]) && isnum(msg[2]) && msg[3] == ' ')
        index = (msg[0] - '0') * 100 + (msg[1] - '0') * 10 + (msg[2] - '0');
    else {
        for (i = 0, index = 0; i < strlen(msg) - 5; i++) {
            if (msg[i] == '\r' &&
                msg[i + 1] == '\n' &&
                isnum(msg[i + 2]) &&
                isnum(msg[i + 3]) &&
                isnum(msg[i + 4]) &&
                msg[i + 5] != '-') {
                index = (msg[i + 2] - '0') * 100 + (msg[i + 3] - '0') * 10 + (msg[i + 4] - '0');
                break;
            }
        }
    }
    switch (index) {
    case 150:
        if (status->status == 14) { // LIST
            int connfd;
            if (status->pasv == 1) { // PASV
                connfd = status->filefd;
            }
            else { // PORT
                struct sockaddr addr;
                socklen_t len;
                connfd = accept(status->filefd, &addr, &len);
                if (connfd == -1) {
                    printf("accept error\n");
                    return;
                }
            }
            while (1) {
                printf("1\n");
                len = read(connfd, buff, sizeof buff);
                if (len == -1) {
                    printf("read error\n");
                    break;
                }
                else if (len == 0) {
                    printf("finish reading\r\n");
                    break;
                }
                fwrite(buff, len, 1, stderr);
            }
            close(connfd);
            if (status->pasv == 0) close(status->filefd);
            status->pasv = -1;
            status->status = 16;
            status->filefd = -1;
        }
        if (status->status == 12) { // RETR
            int connfd;
            if (status->pasv == 1) {
                connfd = status->filefd;
            }
            else {
                struct sockaddr addr;
                socklen_t len;
                connfd = accept(status->filefd, &addr, &len);
                if (connfd == -1) {
                    printf("accept error\n");
                    return;
                }
            }
            //printf("write into file: %s\n", status->filename);
            f = fopen(status->filename, "w");
            if (f == NULL) {
                printf("open error\n");
                return;
            }
            while (1) {
                len = read(connfd, buff, sizeof buff);
                if (len == -1) {
                    printf("read error\n");
                    break;
                }
                else if (len == 0) {
                    break;
                }
                fwrite(buff, len, 1, f);
            }
            close(connfd);
            status->pasv = -1;
            fclose(f);
            status->status = 16;
        }
        if (status->status == 11) { // STOR
            int connfd;
            if (status->pasv == 1) {
                connfd = status->filefd;
            }
            else {
                struct sockaddr addr;
                socklen_t len;
                connfd = accept(status->filefd, &addr, &len);
                if (connfd == -1) {
                    printf("accept error\n");
                    return;
                }
            }
            f = fopen(status->filename, "r");
            if (f == NULL) {
                printf("open error\n");
                return;
            }
            while (1) {
                len = fread(buff, 1, 4096, f);
                if (len == 0) break;
                p = 0;
                while (p < len) {
                    int n = write(connfd, buff + p, len + p);
                    if (n < 0) {
                        printf("Error write(): %s(%d)\n", strerror(errno), errno);
                        return;
                    } else {
                        p += n;
                    }
                }
                if (len != 4096) break;
            }
            close(connfd);
            status->pasv = -1;
            fclose(f);
            status->status = 16;
        }
        break;
    case 200:
        if (status->status == 7) { // TYPE
            status->status = 4;
        }
        if (status->status == 9) { // PORT
            status->pasv = 0;
            if ((status->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                return;
            }
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(status->fileport);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(status->filefd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                printf("Error bind() on %s,%d: %s(%d)\n", status->filehost, status->fileport, strerror(errno), errno);
                return;
            }
            if (listen(status->filefd, 10) == -1) {
                printf("Error listen(): %s(%d)\n", strerror(errno), errno);
                return;
            }
            status->status = 4;
        }
        break;
    case 215:
        if (status->status == 6) { // SYST
            status->status = 4;
        }
        break;
    case 220:
        if (status->status != 1) return;
        if (write(status->sockfd, CUSER, strlen(CUSER)) == -1) {
            printf("write error\n");
            return;
        }
        status->status = 2;
        break;
    case 221:
        status->status = -1;
        break;
    case 226:
        if (status->status == 16) {
            status->status = 4;
        }
        break;
    case 227:
        if (status->status == 8) { // PASV
            status->pasv = 1;
            for (i = 0; i < 100; i++) status->filehost[i] = '\0';
            for (i = 5; i < 100; i++) {
                if (msg[i] >= '0' && msg[i] <= '9') {
                    sscanf(msg+i, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
                    break;
                }
            }
            sprintf(status->filehost, "%d.%d.%d.%d", h1, h2, h3, h4);
            status->fileport = htons(p1 * 256 + p2);
            status->status = 4;
        }
        break;
    case 230:
        if (status->status != 3) return;
        status->status = 4;
        break;
    case 250:
        if (status->status == 13) {
            status->status = 4;
        }
        if (status->status == 15) { // LIST
            status->status = 4;
        }
        if (status->status == 19) { // RNTO
            status->status = 4;
        }
        break;
    case 257:
        if (status->status == 10) { // PWD
            status->status = 4;
        }
        break;
    case 331:
        if (status->status != 2) return;
        if (write(status->sockfd, CPASS, strlen(CPASS)) == -1) {
            printf("write error\n");
            return;
        }
        status->status = 3;
        break;
    case 350:
        if (status->status == 18) { // RNFR
            status->status = 4;
        }
    case 451:
        if (status->status == 11 || status->status == 12) {
            status->status = 4;
        }
        break;
    case 550:
        if (status->status == 13) {
            status->status = 4;
        }
        if (status->status == 15) { // LIST
            status->status = 4;
        }
        if (status->status == 18) { // RNFR
            status->status = 4;
        }
        if (status->status == 19) { // RNTO
            status->status = 4;
        }
        break;
    }
    return;
}

int main(int argc, char **argv) {
    int sockfd;
    Status* status;
	struct sockaddr_in addr;
	char sentence[8192];
	int len;
	int p, i;

    status = (Status*)malloc(sizeof(Status));
    status->status = 0;
    status->path = NULL;
    status->port = htons(21);
    strcpy(status->host, "127.0.0.1");
    status->fileport = -1;
    status->filefd = -1;
    status->gui = 0;
    status->pasv = -1;

    strcpy(CUSER, "USER anonymous\r\n");
    strcpy(CPASS, "PASS abc@mail.com\r\n");

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-user")) sprintf(CUSER, "USER %s\r\n", argv[++i]);
        else if (!strcmp(argv[i], "-pass")) sprintf(CPASS, "PASS %s\r\n", argv[++i]);
        else if (!strcmp(argv[i], "-gui")) status->gui = 1;
        else if (!strcmp(argv[i], "-host")) sprintf(status->host, "%s", argv[++i]);
        else if (!strcmp(argv[i], "-port")) {sscanf(argv[++i], "%d", &status->port); status->port = htons(status->port);}
    }
    
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
    
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = status->port;
	if (inet_pton(AF_INET, status->host, &addr.sin_addr) <= 0) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    status->sockfd = sockfd;
    status->status = 1;

    while (1) {
        if (status->status == 4) {
            if (status->gui != 1)
                printf("command>");
            fgets(sentence, 4096, stdin);
            if (sentence[0] == '\n') continue;
            len = strlen(sentence);
            sentence[len - 1] = '\r';
            sentence[len] = '\n';
            sentence[len + 1] = '\0';
            p = 0;
            if (parsecmd(sentence, status) == -1) {
                continue;
            }
            len = strlen(sentence);
            while (p < len) {
                int n = write(sockfd, sentence + p, len - p);
                if (n < 0) {
                    printf("Error write(): %s(%d)\n", strerror(errno), errno);
                    return 1;
                } else {
                    p += n;
                }
            }
            status->status = parsecmd(sentence, status);
        }
        
        p = 0;
        while (1) {
            int n = read(sockfd, sentence + p, 8191 - p);
            if (n < 0) {
                printf("Error read(): %s(%d)\n", strerror(errno), errno);
                return 1;
            } else if (n == 0) {
                break;
            } else {
                p += n;
                if (sentence[p - 1] == '\n') {
                    break;
                }
            }
        }
        if (p) {
            sentence[p] = '\0';
            printf("%s", sentence);
            replyServer(status, sentence);
        }
        if (status->status == -1)
            break;
    }
	close(sockfd);
	return 0;
}
