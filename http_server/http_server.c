#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

static void die(const char *s){
    perror(s);
    exit(1);
}

int main(int argc, char **argv){
    if(argc != 5){
        fprintf(stderr, "usage: %s <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>\n", argv[0]);
        exit(1);
    }

    unsigned short servport = atoi(argv[1]);
    unsigned short mdbport = atoi(argv[4]);
    struct hostent *he;
    if ((he = gethostbyname(argv[3])) == NULL) {
        die("gethostbyname failed");
    }
        char *mdbIP = inet_ntoa(*(struct in_addr *)he->h_addr);

    // Create sockets
    int servsock;
    if((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        die("socket failed");
    }
    int mdbsock;
    if((mdbsock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        die("socket failed");
    }

    // Construct address structures
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(servport);

    struct sockaddr_in mdbaddr;
    memset(&mdbaddr, 0, sizeof(mdbaddr));
    mdbaddr.sin_family = AF_INET;
    mdbaddr.sin_addr.s_addr = inet_addr(mdbIP);
    mdbaddr.sin_port = htons(mdbport);

    // Establish TCP connections
    if(bind(servsock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        die("bind failed");
    }
    if(connect(mdbsock, (struct sockaddr *)&mdbaddr, sizeof(mdbaddr)) < 0){
        die("connect failed");
    }

    // Start listening for connections
    if(listen(servsock, 5) < 0){
        die("listen failed");
    }
    
    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    while(1){
        clntlen = sizeof(clntaddr);

        if((clntsock = accept(servsock, (struct sockaddr *)&clntaddr, &clntlen)) < 0){
            die("accept failed");
        }

        FILE *client = fdopen(clntsock, "r");
        if(client == NULL){
            die("fdopen failed");
        }
        char buff[4096] = {'\0'};
        char line[1000];

        // read request
        while(fgets(line, sizeof(line), client) != NULL){
            strcat(buff, line);
            if(strstr(buff, "\r\n\r\n") != NULL || strstr(buff, "\n\n") != NULL){
                break;
            }
        }
        if(ferror(client)){
            fprintf(stderr, "ERR: fgets failed\n");
            fclose(client);
            continue;
        } else if(buff[0] == '\0'){
            fclose(client);
            continue;
        }
        char *pt = strchr(buff, '\n');
        int n = pt - buff;
        memcpy(line, buff, n);
        line[n] = '\0';

        char *token_separators = "\t \r\n";
        char *method = strtok(line, token_separators);
        char *requestURI = strtok(NULL, token_separators);
        char *http = strtok(NULL, token_separators);
        char *extra = strtok(NULL, token_separators);
        char *statusCode;
        char message[500];

        // Check if requested file is invalid
        if(method == NULL || requestURI == NULL || http == NULL || extra != NULL || strcmp(method, "GET") != 0 || (strcmp(http, "HTTP/1.0") != 0 && strcmp(http, "HTTP/1.1") != 0)){
            statusCode = "501 Not Implemented";
            int num = sprintf(message, "HTTP/1.0 %s\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>", statusCode);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
            }
            printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);
            fclose(client);
            continue;
        }
        if(requestURI[0] != '/' || strstr(requestURI, "..") != NULL){
            statusCode = "400 Bad Request";
            int num = sprintf(message, "HTTP/1.0 %s\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>", statusCode);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
            }
            printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);
            fclose(client);
            continue;
        }

        // Check if requesting mdb functionality
        const char *form =
            "<h1>mdb-lookup</h1>\n"
            "<p>\n"
            "<form method=GET action=/mdb-lookup>\n"
            "lookup: <input type=text name=key>\n"
            "<input type=submit>\n"
            "</form>\n"
            "<p>\n";
        if(strcmp(requestURI, "/mdb-lookup") == 0){
            statusCode = "200 OK";
            int num = sprintf(message, "HTTP/1.0 %s\r\n\r\n%s", statusCode, form);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
            }
            printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);
            fclose(client);
            continue;
        }

        // Check if requesting mdb query
        const char *tableBegin =
            "<p>\n"
            "<table border=\"\">\n"
            "<tbody>\n";
        const char *tableEnd =
            "</tbody>\n"
            "</table>\n"
            "</p>\n";
        const char *check = "/mdb-lookup?key=";
        if(strncmp(requestURI, check, strlen(check)) == 0){
            statusCode = "200 OK";
            char *key = requestURI + strlen(check);
            int num = snprintf(message, sizeof(message), "%s\n", key);
            for(int i = 0; i < strlen(key); i++){
                if(message[i] == '+'){
                    message[i] = ' ';
                }
            }
            if(send(mdbsock, message, num, 0) != num){
                perror("send failed");
                fclose(client);
                continue;
            }
            num = sprintf(message, "HTTP/1.0 %s\r\n\r\n%s%s", statusCode, form, tableBegin);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
                fclose(client);
                continue;
            }
            printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);
            int flag = 0;
            int count = 0;
            FILE *mdbserv = fdopen(mdbsock, "r");
            if(mdbserv == NULL){
                die("fdopen failed");
            }
            while(fgets(line, sizeof(line), mdbserv) != NULL){
                if(line[0] == '\n'){
                    break;
                }
                if(count % 2 == 0){
                    num = sprintf(message, "<tr>\n<td>%s</td>\n</tr>", line);
                    if(send(clntsock, message, num, 0) != num){
                        perror("send failed");
                        fclose(client);
                        mdbsock = dup(fileno(mdbserv));
                        fclose(mdbserv);
                        flag = 1;
                        break;
                    }
                } else{
                    num = sprintf(message, "<tr>\n<td bgcolor=\"orange\">%s</td>\n</tr>", line);
                    if(send(clntsock, message, num, 0) != num){
                        perror("send failed");
                        fclose(client);
                        mdbsock = dup(fileno(mdbserv));
                        fclose(mdbserv);
                        flag = 1;
                        break;
                    }
                }
                count++;
            }
            if(flag == 1){
                continue;
            }
            if(ferror(mdbserv)){
                fprintf(stderr, "ERR: fgets failed");
                fclose(client);
                mdbsock = dup(fileno(mdbserv));
                fclose(mdbserv);
                continue;
            }
            num = sprintf(message, "%s", tableEnd);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
            }
            fclose(client);
            mdbsock = dup(fileno(mdbserv));
            fclose(mdbserv);
            continue;
        }
    
        // Open requested file
        char filename[100];
        strcpy(filename, argv[2]);
        strcat(filename, requestURI);
        if(filename[strlen(filename) - 1] == '/'){
            strcat(filename, "index.html");
        }
        FILE *requestFile = fopen(filename, "rb");
        if(requestFile == NULL){
            statusCode = "404 Not Found";
            int num = sprintf(message, "HTTP/1.0 %s\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>", statusCode);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
            }
            printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);
            fclose(client);
            continue;
        }
        
        // Check if requested file is a directory
        struct stat sb;
        stat(filename, &sb);
        if(S_ISDIR(sb.st_mode)){
            statusCode = "301 Moved Permanently";
            int num = sprintf(message, "HTTP/1.0 %s\r\nLocation: http://clac.cs.columbia.edu:%s%s/\r\n\r\n<html><body><h1>301 Moved Permanently</h1></body></html>", statusCode, argv[1], requestURI);
            if(send(clntsock, message, num, 0) != num){
                perror("send failed");
            }
            printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);
            fclose(client);
            fclose(requestFile);
            continue;
        }
        
        // Requested file is a valid file
        statusCode = "200 OK";
        printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, http, statusCode);

        // Send request file to client	
        int num = sprintf(message, "HTTP/1.0 %s\r\n\r\n", statusCode);
        if(send(clntsock, message, num, 0) != num){
            perror("send failed");
            fclose(client);
            fclose(requestFile);
            continue;
        }
        int r;
        int flag = 0;
        while((r = fread(buff, 1, sizeof(buff), requestFile)) > 0){
            if(send(clntsock, buff, r, 0) != r){
                perror("send failed");
                fclose(client);
                fclose(requestFile);
                flag = 1;
                break;
            }
        }
        if(flag == 1){
            continue;
        }
        if(ferror(requestFile)){
            perror("fread failed");
            fclose(client);
            fclose(requestFile);
            continue;
        }

        fclose(client);
        fclose(requestFile);
    }

    return 0;
}
