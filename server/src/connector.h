#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "const.h"

#define DEFAULT_PERMISSION 0700

typedef struct Connector{
    char path[100];
    char root[100];
    char host[20];
    char rnname[100];
    int rnflag;
    int port;
    int status;
    int mode;
    int sockfd;
    int filefd;
    struct Connector* next;
}Connector;

/*
status:
-1 unused
00 new connector
01 waiting for USER
02 waiting for PASS
03 waiting for cmd after login

mode:
0 unset
1 pasv
2 port
*/

Connector* createConnectorList() {
    Connector* head = (Connector *)malloc(sizeof(Connector));
    memset(head, 0, sizeof(*head));
    head->port = -1;
    head->status = -1;
    head->mode = -1;
    head->sockfd = -1;
    head->filefd = -1;
    head->rnflag = 0;
    head->next = NULL;
    return head;
}

void appendConnector(Connector* head, int sockfd, char* dir) {
    Connector* node = NULL;
    Connector* tmp = NULL;
    node = (Connector *)malloc(sizeof(Connector));
    memset(node, 0, sizeof(*node));
    node->port = -1;
    snprintf(node->path, 99, "%s", "/");
    snprintf(node->root, 99, "%s", dir);
    node->root[99] = '\0';
    node->status = 1;
    node->sockfd = sockfd;
    node->filefd = -1;
    node->mode = 0;
    node->rnflag = 0;
    tmp = head->next;
    node->next = tmp;
    head->next = node;
}

void deleteConnector(Connector* head, int sockfd) {
    Connector* node = NULL;
    Connector* tmp = NULL;

    for (node = head; node->next != NULL; node = node->next) {
        if (node->next->sockfd == sockfd) {
            tmp = node->next;
            node->next = node->next->next;
            close(tmp->sockfd);
            if (tmp->sockfd != -1) {
                close(tmp->sockfd);
            }
            if(tmp != NULL)
		free(tmp);
            printf("disconnect on descriptor %d\n", tmp->sockfd);
            break;
        }
    }
}

Connector* findConnector(Connector* head, int sockfd) {
    Connector* node = NULL;

    for (node = head; node->next != NULL; node = node->next) {
        if (node->next->sockfd == sockfd) {
            return node->next;
        }
    }
    return NULL;
}

void dot2comma(char *buf) {
  int len = strlen(buf);
  int i = 0;

  for(i = 0; i < len; i++) {
      if (buf[i] == '.') {
        buf[i] = ',';
      }
  }
  return;
}

char* getIP() {
    struct ifaddrs *head = NULL, *iter = NULL;
    if (getifaddrs(&head) == -1) {
        return NULL;
    }
    for (iter = head; iter != NULL; iter = iter->ifa_next) {
        if (iter->ifa_addr == NULL) {
            continue;
        }
        if (iter->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        char mask[INET_ADDRSTRLEN];

        memset(mask, '\0', sizeof(mask));

        void* ptr = &((struct sockaddr_in*) iter->ifa_netmask)->sin_addr;
        inet_ntop(AF_INET, ptr, mask, INET_ADDRSTRLEN);
        if (strcmp(mask, "255.0.0.0") == 0) {
            continue;
        }
        void* tmp = &((struct sockaddr_in *) iter->ifa_addr)->sin_addr;
        char *rlt = (char*)malloc(20);
        memset(rlt, 0, 20);
        inet_ntop(AF_INET, tmp, rlt, INET_ADDRSTRLEN);
        dot2comma(rlt);
        return rlt;
    }
    return NULL;
}

int c2i(char* s) {
    int i = 0;
    int num = 0;
    for (num = 0, i = 0; i < strlen(s); i++) {
        if (s[i] > '9' && s[i] < '0') {
            printf("number out of range\n");
            return -1;
        }
        num = num * 10 + s[i] - '0';
    }
    return num;
}

int iscmd(char* msg, char* cmd) {
    if (strlen(msg) < strlen(cmd)) return 0;
    return strstr(msg, cmd) == msg;
}

char* getcmd(char* msg) {
    int i = 0;
    char *param = NULL;
    param = (char*)malloc((sizeof(char)) * strlen(msg));

    memset(param, '\0', (sizeof(char)) * strlen(msg));

    for (i = 0; i < strlen(msg); i++) {
        if (msg[i] == ' ') {
            snprintf(param, ((sizeof(char)) * strlen(msg)) - 1, "%s", msg + i + 1);
            param[((sizeof(char)) * strlen(msg)) - 1] = '\0';
            break;
        }
    }
    for (i = 0; i < strlen(param); i++) {
        if (param[i] == '\r' || param[i] == '\n' || param[i] == '\0') {
            param[i] = '\0';
            break;
        }
    }
    return param;
}

void USER(Connector* connector) {
    if (connector->status != 1) return;
    if (write(connector->sockfd, S331, strlen(S331)) == -1)
        printf("write error\n");
    else
        connector->status = 2;
}

void PASS(Connector* connector, char* param) {
    int i = 0;
    if (connector->status != 2) return;
    /* after login */
    for (i = 0; i < 6; i++)
        if (write(connector->sockfd, S230s[i], strlen(S230s[i])) == -1) {
            printf("write error\n");
            return;
        }
    connector->status = 3;
}

void SYST(Connector* connector) {
    if (write(connector->sockfd, S215, strlen(S215)) == -1) {
        printf("write error\n");
    }
}

void TYPE(Connector* connector, char* param) {
    if (write(connector->sockfd, S200T, strlen(S200T)) == -1)
        printf("write error\n");
}

void PWD(Connector* connector) {
    char msg[100];
    snprintf(msg, sizeof(msg) - 1, "%s\"%s\"\r\n", S257, connector->path);
    msg[sizeof(msg)-1] = '\0';
    if (write(connector->sockfd, msg, strlen(msg)) == -1) {
        printf("write wrong\n");
        return;
    }
}

void PASV(Connector* connector) {
    int i = 0;
    int sockfd = 0;
    int p1 = 0;
    int p2 = 0;
    struct sockaddr_in addr;
    char msg[100];

    memset(msg, '\0', sizeof(msg));

    memset(&addr, 0, sizeof(addr));
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    for (i = 8192; i < 60000; i++) {
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(i);
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            printf("New connection at port %d\n", i);
            p1 = i / 256;
            p2 = i % 256;
            snprintf(msg, sizeof(msg) - 1, "%s entering(%s,%d,%d)\r\n", S227, getIP(), p1, p2);
            msg[sizeof(msg) - 1] = '\0';
            //           snprintf(msg, sizeof(msg) - 1, "%s127,0,0,1,%d,%d\r\n", S227, p1, p2);
            //           msg[sizeof(msg) - 1] = '\0';
            if (listen(sockfd, 10) == -1) {
                printf("listen failed\n");
                return;
            }
            printf("listen on socket %d\n", sockfd);
            if (write(connector->sockfd, msg, strlen(msg)) == -1) {
                printf("write wrong\n");
                close(sockfd);
                return;
            }
            if (connector->filefd != -1)
                close(connector->filefd);
            connector->filefd = sockfd;
            connector->mode = 1;
            break;
        }
    }
}

void PORT(Connector* connector, char* msg) {
    int h1 = 0, h2 = 0, h3 = 0, h4 = 0, p1 = 0, p2 = 0;
    sscanf(msg, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
    snprintf(connector->host, 19, "%d.%d.%d.%d", h1, h2, h3, h4);
    connector->host[19] = '\0';
    if (connector->filefd != -1)
        close(connector->filefd);
    if (write(connector->sockfd, S200P, strlen(S200P)) == -1) {
        printf("write wrong\n");
        return;
    }
    connector->mode = 2;
    connector->port = p1 * 256 + p2;
}

void STOR(Connector* connector, char* msg) {
    char file[100], filename[100];
    FILE *fin = NULL;
    char buf[512];
    int count = 0, connfd = 0, i = 0;

    memset(buf, '\0', sizeof(buf));
    memset(file, '\0', sizeof(file));
    memset(filename, '\0', sizeof(filename));

    if (connector->mode == 0) {
        printf("need pasv/port\n");
        return;
    }
    for (i = strlen(msg) - 1; i >= -1; i--)
        if (msg[i] == '/' || i == -1) {
            snprintf(filename, sizeof(filename) - 1, "%s", msg + i + 1);
            filename[sizeof(filename) - 1] = '\0';
            break;
        }
    snprintf(file, sizeof(file) - 1, "%s%s/%s", connector->root, connector->path, filename);
    file[sizeof(file) - 1] = '\0';
    fin = fopen(file, "w");
    if (fin == NULL) {
        printf("cannot open file: %s\n", file);
        if (write(connector->sockfd, S451, strlen(S451)) == -1) {
            printf("write wrong\n");
            return;
        }
        return;
    }
    if (connector->mode == 2) { // PORT
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(connector->port);
        if ((connector->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error socket(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (inet_pton(AF_INET, connector->host, &addr.sin_addr) <= 0) {
            printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (connect(connector->filefd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (write(connector->sockfd, S150, strlen(S150)) == -1) {
            printf("write wrong\n");
            return;
        }
        connfd = connector->filefd;
    }
    else if (connector->mode == 1) {
        struct sockaddr addr;
        socklen_t len;
        connfd = accept(connector->filefd, &addr, &len);
        if (connfd == -1) {
            printf("accept error\n");
            return;
        }
        if (write(connector->sockfd, S150, strlen(S150)) == -1) {
            printf("write wrong\n");
            return;
        }
        printf("accepted\n");
    }
    while (1) {
        count = read(connfd, buf, sizeof buf);
        if (count == -1) {
            printf("read error\n");
            break;
        }
        else if (count == 0) {
            printf("finish reading\n");
            break;
        }
        fwrite(buf, count, 1, fin);
    }
    fclose(fin);
    close(connfd);
    close(connector->filefd);
    if (connector->mode == 1) close(connector->filefd);
    connector->mode = 0;
    connector->filefd = -1;
    if (write(connector->sockfd, S226S, strlen(S226S)) == -1) {
        printf("write wrong\n");
        return;
    }
}

void RETR(Connector* connector, char* msg) {
    char file[100];
    FILE *fin = NULL;
    char buf[4096];
    int count = 0, connfd = 0, p = 0;

    memset(buf, '\0', sizeof(buf));
    memset(file, '\0', sizeof(file));

    if (connector->mode == 0) {
        printf("need pasv/port\n");
        return;
    }
    snprintf(file, sizeof(file) - 1, "%s%s/%s", connector->root, connector->path, msg);
    file[sizeof(file) - 1] = '\0';
    fin = fopen(file, "r");
    if (fin == NULL) {
        printf("cannot open file: %s\n", file);
        if (write(connector->sockfd, S451, strlen(S451)) == -1) {
            printf("write wrong\n");
            return;
        }
        return;
    }
    if (connector->mode == 2) { // PORT
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(connector->port);
        if ((connector->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error socket(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (inet_pton(AF_INET, connector->host, &addr.sin_addr) <= 0) {
            printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (connect(connector->filefd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (write(connector->sockfd, S150, strlen(S150)) == -1) {
            printf("write wrong\n");
            return;
        }
        connfd = connector->filefd;
    }
    else if (connector->mode == 1) { // PASV
        struct sockaddr addr;
        socklen_t len;
        connfd = accept(connector->filefd, &addr, &len);
        if (connfd == -1) {
            printf("accept error\n");
            return;
        }
        if (write(connector->sockfd, S150, strlen(S150)) == -1) {
            printf("write wrong\n");
            return;
        }
        printf("accepted\n");
    }
    while (1) {
        count = fread(buf, 1, 4096, fin);
        if (count == 0) break;
        p = 0;
        while (p < count) {
            int n = write(connfd, buf + p, count - p);
            if (n < 0) {
                printf("write wrong\n");
                return;
            }
            else p += n;
        }
        if (count != 4096) break;
    }
    fclose(fin);
    close(connfd);
    if (connector->mode == 1) close(connector->filefd);
    connector->mode = 0;
    connector->filefd = -1;
    if (write(connector->sockfd, S226R, strlen(S226R)) == -1) {
        printf("write wrong\n");
        return;
    }
}

void MKD(Connector* connector, char* param) {
    int done = 0;
    char path[100], cpath[100];
    struct stat st = {0};

    memset(path, '\0', sizeof(path));
    memset(cpath, '\0', sizeof(cpath));

    getcwd(path, 100);
    if (param[0] == '/') {
        snprintf(cpath, sizeof(cpath) - 1, "%s%s", connector->root, param);
        cpath[sizeof(cpath) - 1] = '\0';
    }
    else {
        snprintf(cpath, sizeof(cpath) - 1, "%s%s/%s", connector->root, connector->path, param);
        cpath[sizeof(cpath) - 1] = '\0';
    }


    if (stat(cpath, &st) == -1) {
        mkdir(cpath, DEFAULT_PERMISSION);
        if (write(connector->sockfd, S250, strlen(S250)) == -1) {
            printf("write wrong\n");
            return;
        }
    } else {
        if (write(connector->sockfd, S550C, strlen(S550C)) == -1) {
            printf("write wrong\n");
            return;
        }
    }

    if (chdir(cpath) == 0) {
        done = 1;
        printf("path: %s\n", connector->path);
    }
    else {
        done = 0;
        printf("no path: %s\n", cpath);
    }
    chdir(path);
    if (done) {
        if (write(connector->sockfd, S250, strlen(S250)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
    else {
        if (write(connector->sockfd, S550, strlen(S550)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
}

void RMD(Connector* connector, char* param) {
    int done = 0;
    char path[100], cpath[100];

    memset(path, '\0', sizeof(path));
    memset(cpath, '\0', sizeof(cpath));

    getcwd(path, 100);
    if (param[0] == '/') {
        snprintf(cpath, sizeof(cpath) - 1, "%s%s", connector->root, param);
        cpath[sizeof(cpath) - 1] = '\0';
    }
    else {
        snprintf(cpath, sizeof(cpath) - 1, "%s%s/%s", connector->root, connector->path, param);
        cpath[sizeof(cpath) - 1] = '\0';
    }
    if (chdir(cpath) == 0) {
        done = 1;
        rmdir(cpath);
        printf("path: %s\n", connector->path);
    }
    else {
        done = 0;
        printf("no path: %s\n", cpath);
    }
    chdir(path);
    if (done) {
        if (write(connector->sockfd, S250, strlen(S250)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
    else {
        if (write(connector->sockfd, S550, strlen(S550)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
}

void CWD(Connector* connector, char* param) {
    int done = 0;
    char path[100], cpath[100], croot[100];

    memset(path, '\0', sizeof(path));
    memset(cpath, '\0', sizeof(cpath));
    memset(croot, '\0', sizeof(croot));

    getcwd(path, 100);
    if (param[0] == '/') {
        snprintf(cpath, sizeof(cpath) - 1, "%s%s", connector->root, param);
        cpath[sizeof(cpath) - 1] = '\0';
    }
    else {
        snprintf(cpath, sizeof(cpath) - 1, "%s%s/%s", connector->root, connector->path, param);
        cpath[sizeof(cpath) - 1] = '\0';
    }
    if (chdir(cpath) == 0) {
        done = 1;
        getcwd(cpath, 100);
        snprintf(croot, sizeof(croot) - 1, "%s/", croot);
        croot[sizeof(croot) - 1] = '\0';
        if (strstr(croot, cpath) == croot) done = 1;
        else if (strstr(cpath, connector->root) != cpath) done = 0;
        else {
            snprintf(connector->path, 99, "%s", cpath + strlen(connector->root));
            connector->path[99] = '\0';
            if (strlen(connector->path) == 0)
                snprintf(connector->path, 99, "%s", "/");
        }
        printf("path: %s\n", connector->path);
    }
    else {
        done = 0;
        printf("no path: %s\n", cpath);
    }
    chdir(path);
    if (done) {
        if (write(connector->sockfd, S250, strlen(S250)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
    else {
        if (write(connector->sockfd, S550C, strlen(S550C)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
}

void DELE(Connector* connector, char* param) {
    char path[200];
    FILE *fin = NULL;

    memset(path, '\0', sizeof(path));

    if (param[0] == '/') {
        snprintf(path, sizeof(path) - 1, "%s%s", connector->root, param);
        path[sizeof(path) - 1] = '\0';
    }
    else {
        snprintf(path, sizeof(path) - 1, "%s%s/%s", connector->root, connector->path, param);
        path[sizeof(path) - 1] = '\0';
    }
    fin = fopen(path, "r");
    if (fin == NULL) {
        if (write(connector->sockfd, S550, strlen(S550)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
    else {
        fclose(fin);
        unlink(path);
        if (write(connector->sockfd, S250, strlen(S250)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
}

void LIST(Connector* connector) {
    char buf[4096];
    char cmdls[200];
    int count = 0, connfd = 0, p = 0;

    FILE *fname = NULL;

    memset(buf, '\0', sizeof(buf));
    memset(cmdls, '\0', sizeof(cmdls));

    if (connector->mode == 0) {
        printf("need pasv/port\n");
        return;
    }
    if (connector->mode == 2) { // PORT
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(connector->port);
        if ((connector->filefd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error socket(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (inet_pton(AF_INET, connector->host, &addr.sin_addr) <= 0) {
            printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (connect(connector->filefd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        if (write(connector->sockfd, S150, strlen(S150)) == -1) {
            printf("write wrong\n");
            return;
        }
        connfd = connector->filefd;
    }
    else if (connector->mode == 1) { // PASV
        connfd = accept(connector->filefd, NULL, NULL);
        if (connfd == -1) {
            printf("accept error\n");
            return;
        }
        if (write(connector->sockfd, S150, strlen(S150)) == -1) {
            printf("write wrong\n");
            return;
        }
        printf("accepted\n");
    }

    snprintf(cmdls, sizeof(cmdls) - 1, "ls -l %s%s", connector->root, connector->path);
    cmdls[sizeof(cmdls) - 1] = '\0';
    fname = popen(cmdls, "r");
    while (1) {
        count = fread(buf, 1, 4096, fname);
        if (count == 0) break;
        p = 0;
        while (p < count) {
            int n = write(connfd, buf + p, count - p);
            if (n < 0) {
                printf("write wrong\n");
                return;
            }
            else p += n;
            }
        if (count != 4096) break;
    }
    pclose(fname);
    close(connfd);
    if (connector->mode == 1) close(connector->filefd);
    connector->mode = 0;
    connector->filefd = -1;
    if (write(connector->sockfd, S226L, strlen(S226L)) == -1) {
        printf("write wrong\n");
        return;
    }
}

void CDUP(Connector* connector, char* param) {
    CWD(connector, "..");
}

void RNFR(Connector* connector, char* param) {
    /* potential danger, without checking the path*/
    char path[200];
    FILE *fin = NULL;

    memset(path, '\0', sizeof(path));

    if (param[0] == '/') {
        snprintf(path, sizeof(path) - 1, "%s%s", connector->root, param);
        path[sizeof(path) - 1] = '\0';
    }
    else {
        snprintf(path, sizeof(path) - 1, "%s%s/%s", connector->root, connector->path, param);
        path[sizeof(path) - 1] = '\0';
    }
    fin = fopen(path, "r");
    if (fin == NULL) {
        if (write(connector->sockfd, S550, strlen(S550)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
    else {
        fclose(fin);
        snprintf(connector->rnname, 99, "%s", path);
        connector->rnname[99] = '\0';
        connector->rnflag = 2;
        if (write(connector->sockfd, S350, strlen(S350)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
}

void RNTO(Connector* connector, char* param) {
    char path[200];
    FILE *fin = NULL;

    memset(path, '\0', sizeof(path));

    if (param[0] == '/') {
        snprintf(path, sizeof(path) - 1, "%s%s", connector->root, param);
        path[sizeof(path) - 1] = '\0';
    }
    else {
        snprintf(path, sizeof(path) - 1, "%s%s/%s", connector->root, connector->path, param);
        path[sizeof(path) - 1] = '\0';
    }
    if (connector->rnflag != 1) {
        if (write(connector->sockfd, S503, strlen(S503)) == -1) {
            printf("write wrong\n");
            return;
        }
        return;
    }
    rename(connector->rnname, path);
    fin = fopen(path, "r");
    if (fin == NULL) {
        if (write(connector->sockfd, S550, strlen(S550)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
    else {
        fclose(fin);
        snprintf(connector->rnname, 99, "%s", path);
        connector->rnname[99] = '\0';
        if (write(connector->sockfd, S250, strlen(S250)) == -1) {
            printf("write wrong\n");
            return;
        }
    }
}

void QUIT(Connector* connector, Connector* connList) {
    if (write(connector->sockfd, S221, strlen(S221)) == -1) {
        close(connector->sockfd);
        deleteConnector(connList, connector->sockfd);
        printf("Closed connection on descriptor %d\n", connector->sockfd);
    }
}

void responseClient(Connector* connList, int sockfd, char* msg) {
    Connector* connector = NULL;
    char *param = NULL;

    connector = findConnector(connList, sockfd);
    param = getcmd(msg);
    if (connector->rnflag != 0) connector->rnflag--;
    if (iscmd(msg, "USER")) USER(connector);
    else if (iscmd(msg, "PASS")) PASS(connector, param);
    else if (iscmd(msg, "SYST")) SYST(connector);
    else if (iscmd(msg, "TYPE")) TYPE(connector, param);
    else if (iscmd(msg, "PASV")) PASV(connector);
    else if (iscmd(msg, "PORT")) PORT(connector, param);
    else if (iscmd(msg, "STOR")) STOR(connector, param);
    else if (iscmd(msg, "RETR")) RETR(connector, param);
    else if (iscmd(msg, "CDUP")) CDUP(connector, param);
    else if (iscmd(msg, "LIST")) LIST(connector);
    else if (iscmd(msg, "DELE")) DELE(connector, param);
    else if (iscmd(msg, "RNFR")) RNFR(connector, param);
    else if (iscmd(msg, "RNTO")) RNTO(connector, param);
    else if (iscmd(msg, "CWD")) CWD(connector, param);
    else if (iscmd(msg, "RMD")) RMD(connector, param);
    else if (iscmd(msg, "MKD")) MKD(connector, param);
    else if (iscmd(msg, "PWD")) PWD(connector);
    else QUIT(connector, connList);
    if(param != NULL)
    	free(param);
}
