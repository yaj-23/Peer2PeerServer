/* A P2P client
It provides the following functions:
- Register the content file to the index server (R)
- Contact the index server to search for a content file (D)
        - Contact the peer to download the file
        - Register the content file to the index server
- De-register a content file (T)
- List the local registered content files (L)
- List the on-line registered content files (O)
*/
#define _XOPEN_SOURCE 700
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#define QUIT "quit"
#define SERVER_PORT 10000 /* well-known port */
#define BUFLEN 100        /* buffer length */
#define NAMESIZ 10
#define MAXCON 200

#define h_addr h_addr_list[0]

typedef struct {
    char type;
    char data[BUFLEN];
} PDU;
PDU rpdu;

struct {
    int val;
    char name[NAMESIZ];
} table[MAXCON];  // Keep Track of the registered content

int maxIndex = 0;
char usr[NAMESIZ];

int s_sock, peer_port;
int fd, nfds;
fd_set rfds, afds;

void registration(int, char *, struct sockaddr_in);
int search_content(int, char *, PDU *, struct sockaddr_in);
int client_download(char *, PDU *);
void server_download();
void deregistration(int, char *, struct sockaddr_in);
void online_list(int, struct sockaddr_in);
void local_list();
void quit(int, struct sockaddr_in);
void handler(struct sockaddr_in);

int main(int argc, char **argv) {
    int s_port = SERVER_PORT;
    int n;
    int alen = sizeof(struct sockaddr_in);
    struct hostent *hp;
    struct sockaddr_in server;
    char c, *host, name[NAMESIZ];
    struct sigaction sa;

    switch (argc) {
        case 2:
            host = argv[1];
            break;
        case 3:
            host = argv[1];
            s_port = atoi(argv[2]);
            break;
        default:
            printf("Usage: %s host [port]\n", argv[0]);
            exit(1);
    }

    /* UDP Connection with the index server		*/
    memset(&server, 0, alen);
    server.sin_family = AF_INET;
    server.sin_port = htons(s_port);
    if (hp = gethostbyname(host))
        memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
    else if ((server.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
        printf("Can't get host entry \n");
        exit(1);
    }
    s_sock = socket(PF_INET, SOCK_DGRAM,
                    0);  // Allocate a socket for the index server
    if (s_sock < 0) {
        printf("Can't create socket \n");
        exit(1);
    }
    if (connect(s_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Can't connect \n");
        exit(1);
    }

    /* 	Enter User Name		*/
    printf("Choose a user name\n");
    scanf("%s", usr);

    /* Initialization of SELECT`structure and table structure	*/
    FD_ZERO(&afds);
    FD_SET(0, &afds); /* Listening on the read descriptor   */

    FD_SET(s_sock, &afds); /* Listening on the index server socket  */
    nfds = 0;
    for (n = 0; n < MAXCON; n++) {
        table[n].val = -1;
    }

    /*	Setup signal handler		*/
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    /* Main Loop	*/
    while (1) {
        printf("Command:\n");

        memcpy(&rfds, &afds, sizeof(rfds));
        if (select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0) {
            printf("select error: %s\n", strerror(errno));
            exit(1);
        }

        if (FD_ISSET(0, &rfds)) { /* Command from the user  */
            c = getchar();

            /*	Command options	*/
            if (c == '?') {
                printf(
                    "R-Content Registration; T-Content Deregistration; L-List "
                    "Local Content\n");
                printf(
                    "D-Download Content; O-List all the On-line Content; "
                    "Q-Quit\n\n");
                continue;
            }

            /*	Content Regisration	*/
            if (c == 'R') {
                FILE *fp = NULL;
                char filename[10];
                printf("\nEnter the filename you want to register: ");
                scanf("%s", filename);
                printf("%s", filename);

                fp = fopen(filename, "rb");
                if (fp == NULL) {
                    printf("Invalid File Name\n");
                } else {
                    fclose(fp);
                    registration(s_sock, filename, server);
                }
            }

            /*	List Content		*/
            if (c == 'L') {
                local_list();
            }

            /*	List on-line Content	*/
            if (c == 'O') {
                online_list(s_sock, server);
                /* Call online_list()	*/
            }

            /*	Download Content	*/
            if (c == 'D') {
                /* Call search_content()	*/
                char filename[10];
                FILE *fp = NULL;

                printf("\nEnter the filename you want to download: ");
                PDU *rpdu = malloc(sizeof(PDU));
                scanf("%s", filename);
                search_content(s_sock, filename, rpdu, server);
                client_download(filename, rpdu);
                /* Call client_download()	*/
                /* Call registration()		*/
            }

            /*	Content Deregistration	*/
            if (c == 'T') {
                for (int i = 0; i < maxIndex; i++) {
                    if (strcmp(table[i].name, "")) {
                        deregistration(s_sock, table[i].name, server);
                    }
                }
            }

            /*	Quit	*/
            if (c == 'Q') {
                quit(s_sock, server);
            }
        } else {
            fprintf(stderr, "reached");
            server_download(s_sock);
        }
        /* Content transfer: Server to client		*/
    }
}

void quit(int s_sock, struct sockaddr_in server) {
    for (int i = 0; i < maxIndex; i++) {
        if (strcmp(table[i].name, "")) {
            deregistration(s_sock, table[i].name, server);
        }
    }
    exit(0);
}

void local_list() { /* List local content	*/
    for (int i = 0; i < maxIndex; i++) {
        if (strcmp(table[i].name, "")) {
            printf("[%d] %s\n", i + 1, table[i].name);
        }
    }
}

void online_list(int s_sock, struct sockaddr_in sin) {
    /* Contact index server to acquire the list of content */
    // making the OPDU to send to the index server
    PDU *opdu = malloc(sizeof(PDU));
    bzero(opdu->data, 100);
    opdu->type = 'O';

    // send OPDU to index server
    int len;
    sendto(s_sock, opdu, sizeof(PDU), 0, (struct sockaddr *)&sin, sizeof(sin));

    // recieve data back from index server
    PDU recievedPDU;
    while (recievedPDU.type != 'F') {
        recvfrom(s_sock, &recievedPDU, sizeof(PDU), 0, (struct sockaddr *)&sin,
                 &len);
        printf("%s\n", &(recievedPDU.data));
    }

    // just printing everything in the pdu for now
}

void server_download(
    int s_sock) { /* Respond to the download request from a peer    */
    fprintf(stderr, "reached\n");
    for (int i = 0; i < maxIndex; i++) {
        if (strcmp(table[i].name, "")) {
            fprintf(stderr, "reached\n");
            // printf("[%d] %s\n", i + 1, table[i].name);
            if (FD_ISSET(table[i].val, &rfds)) {
                struct sockaddr_in client;
                int client_len = sizeof(client);
                int new_sd = accept(table[i].val, (struct sockaddr *)&client,
                                    &client_len);
                if (new_sd < 0) {
                    fprintf(stderr, "Can't accept client \n");
                    exit(1);
                }
                PDU recPDU;
                char file[100];
                char ch;
                char packet[100];
                char error[100];
                int n = 0;

                n = read(new_sd, &recPDU, sizeof(PDU));
                strcpy(file, recPDU.data);
                printf("File Requested: %s\n", recPDU.data);
                FILE *fptr = fopen(recPDU.data, "rb");
                if (fptr == NULL) {
                    printf("File not found.\n");
                    int i;
                    for (i = 0; i < 5; i++) {
                        error[i] = 'E';
                    }
                    write(new_sd, error, 100);
                    close(s_sock);
                    exit(1);
                }

                PDU *contentPDU = malloc(sizeof(PDU));
                contentPDU->type = 'C';
                while ((n = fread(contentPDU->data, 1, 100, fptr)) > 0) {
                    printf("Packet size: %d\n", n);
                    if (write(new_sd, contentPDU, n + 1) < 0) {
                        printf("\nWrite ERROR\n");
                        fclose(fptr);
                        close(new_sd);
                        break;
                    }
                    printf("Data sent: %s\n", contentPDU->data);
                    bzero(contentPDU->data, 100);
                }

                fclose(fptr);
                close(new_sd);
                break;
            }
        }
    }
}

int search_content(int s_sock, char *name, PDU *spdu, struct sockaddr_in sin) {
    /* Contact index server to search for the content
       If the content is available, the index server will return
       the IP address and port number of the content server.	*/
    // creating an spdu to send to index server which contains filename

    bzero(spdu->data, 100);
    spdu->type = 'S';
    strcpy(spdu->data, name);
    // sending the spdu to index server
    sendto(s_sock, spdu, sizeof(*spdu), 0, (const struct sockaddr *)&sin,
           sizeof(sin));

    // recieve data from index server
    bzero(spdu->data, 100);
    int len;
    recvfrom(s_sock, spdu, sizeof(PDU), 0, (const struct sockaddr *)&sin, &len);
    printf("%s\n", spdu->data);
}

int client_download(char *name, PDU *pdu) {
    /* Make TCP connection with the content server to initiate the
       Download.    */

    // int port = 3000;
    struct hostent *phe;    /* pointer to host information entry    */
    struct sockaddr_in sin; /* an Internet endpoint address        */

    char *res = strtok(pdu->data, ":");
    char *ip = malloc(sizeof(sin.sin_addr));
    bzero(ip, 15);
    strcpy(ip, res);
    // fprintf(stderr, "%s\n", user);
    //  user = get_Username(data);
    char *port = malloc(sizeof(sin.sin_port));
    bzero(port, 5);
    res = strtok(NULL, "\n");
    strcpy(port, res);

    int alen = sizeof(sin);
    int sd, s, n, type; /* socket descriptor and socket type    */

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(port));

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Can't creat a socket\n");
        exit(1);
    }

    /* Map host name to IP address, allowing for dotted decimal */
    if (phe = gethostbyname(pdu->data))
        bcopy(phe->h_addr_list[0], (char *)&sin.sin_addr, phe->h_length);
    else if (inet_aton(ip, (struct in_addr *)&sin.sin_addr)) {
        fprintf(stderr, "Can't get server's address\n");
        exit(1);
    }

    if (connect(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        fprintf(stderr, "Can't connect \n");
        exit(1);
    } else {
        printf("Connected\n");

        // Setting up data PDU to send to server
        PDU *dataPDU = malloc(sizeof(PDU));
        bzero(dataPDU->data, 100);
        dataPDU->type = 'D';
        strcpy(dataPDU->data, name);
        printf("data: %s\n", dataPDU->data);
        printf("type: %c\n", dataPDU->type);
        // dataPDU->data[strlen(name)] = '\0';
        write(sd, dataPDU, sizeof(PDU));

        FILE *fptr = fopen(dataPDU->data, "w");

        // Server will send back content pdu
        PDU *contentPDU = malloc(sizeof(PDU));
        bzero(contentPDU->data, 100);
        int len, n;

        while ((n = read(sd, contentPDU, sizeof(PDU))) > 0) {
            if (contentPDU->type == 'E') {
                printf("Server Error: %s\n", contentPDU->data + 1);
                remove(name);
                break;
            }
            printf("Data Recieved: %s\n", contentPDU->data);
            fwrite(contentPDU->data, 1, n - 1, fptr);
            bzero(contentPDU->data, 100);
        }
        fclose(fptr);
        close(sd);
        printf("Socket Closed!\n");
    }
}

void deregistration(int s_sock, char *name, struct sockaddr_in server) {
    /* Contact the index server to deregister a content registration; Update
     * nfds. */
    PDU *tPDU = malloc(sizeof(PDU));

    bzero(tPDU->data, 100);
    tPDU->type = 'T';
    strcat(tPDU->data, name);
    strcat(tPDU->data, "|");
    strcat(tPDU->data, usr);

    sendto(s_sock, tPDU, sizeof(PDU), 0, (const struct sockaddr *)&server,
           sizeof(server));

    PDU recPDU;
    int i, len;
    recvfrom(s_sock, &recPDU, sizeof(PDU), 0, (struct sockaddr *)&server, &len);
    printf("%s\n", recPDU.data);

    for (i = 0; i < maxIndex; i++) {
        if (!strcmp(name, table[i].name)) {
            strcpy(table[i].name, "");
            FD_CLR(table[i].val, &afds);
            close(table[i].val);
        }
    }
}

void registration(int s_sock, char *name, struct sockaddr_in server) {
    /* Create a TCP socket for content download
                    � one socket per content;
       Register the content to the index server;
       Update nfds;	*/

    struct sockaddr_in sin; /* an Internet endpoint address */
    int alen = sizeof(sin);

    int s, len, n, i; /* socker descriptor */
    char port[10];

    /* Allocate a socket, in TCP*/
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(0);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *)&sin, alen) == -1) {
        fprintf(stderr, "Can't bind name to socket\n");
        exit(1);
    }

    listen(s, 3);

    int blen = sizeof(struct sockaddr_in);
    getsockname(s, (struct sockaddr *)&sin, &blen);
    sprintf(port, "%d", ntohs(sin.sin_port));

    PDU *regPDU = malloc(sizeof(PDU));
    bzero(regPDU->data, 100);
    regPDU->type = 'R';
    // fill out 10 bytes, remove delimiter
    strcat(regPDU->data, usr);
    strcat(regPDU->data, "|");
    strcat(regPDU->data, name);
    strcat(regPDU->data, "|");
    //  combine the port and addr fields
    //  strcat(regPDU->data, inet_ntoa(sin.sin_addr));
    strcat(regPDU->data, port);
    strcat(regPDU->data, "|");

    fprintf(stderr, "%s\n", regPDU->data);
    sendto(s_sock, regPDU, sizeof(*regPDU), 0, (const struct sockaddr *)&server,
           sizeof(server));

    PDU recPDU;
    recvfrom(s_sock, &recPDU, sizeof(PDU), 0, (const struct sockaddr *)&server,
             sizeof(server));

    if (recPDU.type == 'A') {
        strcpy(table[maxIndex].name, name);
        for (int i = 0; i < maxIndex; i++) {
            if (strcmp(table[i].name, "") && s >= nfds) {
                nfds = s + 1;
            }
        }
        table[maxIndex].val = s;
        FD_SET(s, &afds);
        maxIndex++;
        fprintf(stderr, "A: %s\n", recPDU.data);
    } else if (recPDU.type == 'E') {
        fprintf(stderr, "Error:\n");
    } else {
        fprintf(stderr, "NUll: %s\n", recPDU.data);
    }
}

void handler(struct sockaddr_in server) { quit(s_sock, server); }