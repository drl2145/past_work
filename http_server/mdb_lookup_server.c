#include <ctype.h>
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
#include <mylist.h>
#include "mdb.h"

static void die(const char *s){
    perror(s);
    exit(1);
}

int main(int argc, char **argv){
    if(argc != 3){
        fprintf(stderr, "usage: %s <database> <port number>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[2]);

    // Create server socket	
    int servsock;
    if((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        die("socket failed");
    }

    // Construct local address structure
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Bind to the local address
    if(bind(servsock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
        die("bind failed");
    }

    // Start listening for connections
    if(listen(servsock, 5) < 0){
        die("listen failed");
    }

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    while(1){
        fprintf(stderr, "waiting ...\n");

        clntlen = sizeof(clntaddr);

        if((clntsock = accept(servsock, (struct sockaddr *)&clntaddr, &clntlen)) < 0){
            die("accept failed");
        }

        fprintf(stderr, "Connection!\n");

        FILE *client = fdopen(clntsock, "r");

        // Create database file
        char *filename = argv[1];
        FILE *fp = fopen(filename, "rb");
        if(fp == NULL){
            die(filename);
        }

        // Create database list
        struct List list;
        initList(&list);
        struct Node *node = NULL;

        // Read records into memory
        struct MdbRec *pRec = malloc(sizeof(struct MdbRec) + sizeof(int));
        if(pRec == NULL){
            die("malloc returned NULL");
        }

        int record_num = 1;

        while(fread(pRec, sizeof(struct MdbRec), 1, fp) != 0){
            struct MdbRec *mdbp = pRec;
            mdbp++;
            int *ip = (int *)mdbp;
            *ip = record_num++;
            node = addAfter(&list, node, pRec);
            pRec = malloc(sizeof(struct MdbRec) + sizeof(int));
            if(pRec == NULL){
                die("malloc returned NULL");
            }
        }
        if(ferror(fp)){
            perror(filename);
            exit(1);
        }
        free(pRec);
        fclose(fp);

        // Take in user input
        char buff[1000];
        char input[6];
        char newline[1] = {'\n'};
        while(fgets(buff, sizeof(buff), client) != NULL){
            strncpy(input, buff, 5);
            input[5] = '\0';
            
            // Remove newline
            size_t last = strlen(input) - 1;
            if(input[last] == '\n'){
                input[last] = '\0';
            }

            // Traverse list and print records
            node = list.head;
            while(node){
                char output[100];
                int num;
                struct MdbRec *rec = (struct MdbRec *)node->data;
                struct MdbRec *point = (struct MdbRec *)node->data;
                point++;

                if(strstr(rec->name, input) || strstr(rec->msg, input)){
                    num = snprintf(output, sizeof(output), "%4d: {%s} said {%s}\n", *(int *)point, rec->name, rec->msg);
                    if(send(clntsock, output, num, 0) != num){
                        fprintf(stderr, "ERR: send failed\n");
                    }
                }
                node = node->next;
            }
            if(send(clntsock, newline, sizeof(newline), 0) != sizeof(newline)){
                fprintf(stderr, "ERR: send failed\n");
            }
        }
        if(ferror(client)){
            fprintf(stderr, "%s\n", "Input error");
        }

        // Free memory
        traverseList(&list, free);
        removeAllNodes(&list);
        fclose(client);
    }

    return 0;
}
