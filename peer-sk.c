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
#define NAMESIZ 20
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

char usr[NAMESIZ];

int s_sock, peer_port;
int fd, nfds;
fd_set rfds, afds;

void registration(int, char *);
int search_content(int, char *, PDU *, struct sockaddr_in);
int client_download(char *, PDU *);
void server_download();
void deregistration(int, char *);
void online_list(int, struct sockaddr_in);
void local_list();
void quit(int);
void handler();

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
    FD_SET(s_sock, &afds); /* Listening on the index server socket  */
    FD_SET(0, &afds);      /* Listening on the read descriptor   */
    nfds = 1;
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
        if (select(nfds, &rfds, NULL, NULL, NULL) == -1) {
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
                /* Call registration() */
            }

            /*	List Content		*/
            if (c == 'L') {
                /* Call local_list()	*/
            }

            /*	List on-line Content	*/
            if (c == 'O') {
                /* Call online_list()	*/
            }

            /*	Download Content	*/
            if (c == 'D') {
                /* Call search_content()	*/
                char filename[10];
                FILE *fp = NULL;

                printf("\nEnter the filename you want to download: ");
                scanf("%s", filename);
                // search_content(s_sock, filename, )
                /* Call client_download()	*/
                /* Call registration()		*/
            }

            /*	Content Deregistration	*/
            if (c == 'T') {
                /* Call deregistration()	*/
            }

            /*	Quit	*/
            if (c == 'Q') {
                /* Call quit()	*/
            }

        }

        /* Content transfer: Server to client		*/
        else
            server_download(s_sock);
    }
}

void quit(int s_sock) {
    /* De-register all the registrations in the index server	*/
}

void local_list() { /* List local content	*/
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
    int len;
    recvfrom(s_sock, &recievedPDU, sizeof(PDU), 0, (struct sockaddr *)&sin,
             &len);

    // just printing everything in the pdu for now
    printf("%s/n", &(recievedPDU.data));
}

void server_download() { /* Respond to the download request from a peer	*/
}

int search_content(int s_sock, char *name, PDU *rpdu, struct sockaddr_in sin) {
    /* Contact index server to search for the content
       If the content is available, the index server will return
       the IP address and port number of the content server.	*/
    // creating an spdu to send to index server which contains filename
    PDU *spdu = malloc(sizeof(PDU));
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
       Download.	*/
    char *host;
    int port = 3000;
    struct hostent *phe;    /* pointer to host information entry	*/
    struct sockaddr_in sin; /* an Internet endpoint address		*/
    int alen = sizeof(sin);
    int s, n, type; /* socket descriptor and socket type	*/

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    /* Map host name to IP address, allowing for dotted decimal */
    if (phe = gethostbyname(host))
        bcopy(phe->h_addr_list[0], (char *)&sin.sin_addr, phe->h_length);
    else if (inet_aton(host, (struct in_addr *)&sin.sin_addr)) {
        fprintf(stderr, "Can't get server's address\n");
        exit(1);
    }

    if (connect(s_sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        fprintf(stderr, "Can't connect \n");
        exit(1);
    } else {
        // Setting up data PDU to send to server
        PDU *dataPDU = malloc(sizeof(PDU));
        bzero(dataPDU->data, 100);
        dataPDU->type = 'D';
        strcpy(dataPDU->data, name);

        write(s_sock, dataPDU->data, 100);

        FILE *fptr = fopen(dataPDU->data, "wb");

        // Server will send back content pdu
        PDU *contentPDU = malloc(sizeof(PDU));
        bzero(contentPDU->data, 100);
        int len, n;

        while ((n = read(s_sock, contentPDU->data, sizeof(contentPDU->data))) >
               0) {
            if (contentPDU->data[0] == 'E') {
                printf("Server Error: %s\n", contentPDU->data + 1);
                remove(name);
                break;
            }
            fwrite(contentPDU->data, 1, n, fptr);
        }
        fclose(fptr);
        close(s_sock);
        printf("Socket Closed!\n");
    }
}

void deregistration(int s_sock, char *name) {
    /* Contact the index server to deregister a content registration; Update
     * nfds. */
}

void registration(int s_sock, char *name) {
    /* Create a TCP socket for content download
                    � one socket per content;
       Register the content to the index server;
       Update nfds;	*/
}

void handler() { quit(s_sock); }
