/* Index Server

Message types:
R - used for registration
A - used by the server to acknowledge the success of registration
Q - used by chat users for de-registration
D - download content between peers (not used here)
C - Content (not used here)
S - Search content
L - Location of the content server peer
E - Error messages from the Server

*/
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define MSG1 "Cannot find content"
#define MSG2 "Request was Successful"
#define BUFLEN 100
#define NAMESIZ 20
#define MAX_NUM_CON 200

typedef struct entry {
    char usr[NAMESIZ];
    struct sockaddr_in addr;
    short token;
    struct entry *next;
} ENTRY;
// make to enter into List struct
typedef struct {
    char name[NAMESIZ];
    ENTRY *head;
} LIST;
LIST list[MAX_NUM_CON];
int max_index = 0;
// every new piece of content we incement the max_index by 1
typedef struct {
    char type;
    char data[BUFLEN];
} PDU;

// used for transferring content=>server connection info

void search(int, char *, struct sockaddr_in *);
void registration(int, char *, struct sockaddr_in *);
void deregistration(int, char *, struct sockaddr_in *);

////////////////////Helpers///////////////////////////
ENTRY search_Entry(ENTRY *head);
ENTRY *create_Entry(char *data, struct sockaddr_in addr);
void send_Packet(int s, char type, char *content, struct sockaddr_in addr);
void send_Error(int s, struct sockaddr_in addr);
PDU *create_Packet(char type, char *content);
char *get_Username(char *data);
char *get_ContentName(char *data);
int add_Entry(struct entry *head, char *user, struct sockaddr_in *addr);
void content_List(int s, char *strings[], size_t size, struct sockaddr_in addr);
int delete_Entry(struct entry *head, char *user, struct sockaddr_in *addr);
void send_Ack(int s, struct sockaddr_in addr);
char *Addr_to_char(struct sockaddr_in addr);
//////////////////////////////////////////////////////

/*
 *------------------------------------------------------------------------
 * main - Iterative UDP server for Content Indexing service
 *------------------------------------------------------------------------
 */

int main(int argc, char *argv[]) {
    struct sockaddr_in sin, *p_addr; /* the from address of a client	*/
    ENTRY *p_entry;

    char *service = "10000"; /* service name or port number	*/
    char name[NAMESIZ], usr[NAMESIZ];
    int alen = sizeof(struct sockaddr_in); /* from-address length
                                            */
    int s, n, i, len, p_sock; /* socket descriptor and socket type    */
    int pdulen = sizeof(PDU);
    struct hostent *hp;
    PDU rpdu;
    struct sockaddr_in fsin; /* the from address of a client	*/

    for (n = 0; n < MAX_NUM_CON; n++) list[n].head = NULL;

    switch (argc) {
        case 1:
            break;
        case 2:
            service = argv[1];
            break;
        default:
            fprintf(stderr, "usage: chat_server [host [port]]\n");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;

    /* Map service name to port number */
    sin.sin_port = htons((unsigned)atoi(service));

    /* Allocate a socket */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        fprintf(stderr, "can't creat socket\n");
        exit(1);
    }

    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        fprintf(stderr, "can't bind to %s port\n", service);

    while (1) {
        fprintf(stderr, "Waiting for Packet: \n");
        if ((n = recvfrom(s, &rpdu, pdulen, 0, (struct sockaddr *)&fsin,
                          &alen)) < 0)
            printf("recvfrom error: n=%d\n", n);

        /*	Content Registration Request			*/
        if (rpdu.type == 'R') {
            registration(s, rpdu.data, &fsin);
            /*	Call registration()
             */
        }

        /* Search Content		*/
        if (rpdu.type == 'S') {
            /* Call search()
             */
            search(s, rpdu.data, &fsin);
        }

        /*	List current Content */
        if (rpdu.type == 'O') {
            /* Read from the content list and send the list to the
               client 		*/
            char *content = malloc(10 * (max_index));
            content_List(s, content, max_index, fsin);
        }

        /*	De-registration		*/
        if (rpdu.type == 'T') {
            deregistration(s, rpdu.data, &fsin);
            /* Call deregistration()
             */
        } else {
            continue;
        }
    }
    return 0;
}

void search(int s, char *data, struct sockaddr_in *addr) {
    /* Search content list and return the answer:
       If found, send the address of the selected content server.
    */
    int i;
    ENTRY res;

    for (i = 0; i < max_index; i++) {
        if (strcmp(list[i].name, data) == 0) {
            res = search_Entry(list[i].head);
            fprintf(stderr, "Port check: %d\n", ntohs(res.addr.sin_port));
            fprintf(stderr, "address check: %s\n",
                    inet_ntoa(res.addr.sin_addr));
            send_Packet(s, 'S', Addr_to_char(res.addr), *addr);
            return;
        }
    }
    // if reached here, send error packet, content doesnt exist or no more
    //  servers are registered
    send_Error(s, *addr);
    return;
}

void deregistration(int s, char *data, struct sockaddr_in *addr) {
    /* De-register the server of that content
     */
    char *res = strtok(data, "|");
    char contentName[10];

    strcpy(contentName, res);
    // fprintf(stderr, "%s\n", user);
    //  user = get_Username(data);
    char user[10];
    res = strtok(NULL, "|");
    // contentName = get_ContentName(data);
    strcpy(user, res);

    int i = 0;
    int j = max_index;
    for (i = 0; i <= max_index; i++) {
        // content name exists in list
        if (strcmp(list[i].name, contentName) == 0) {
            // look through list to see if user is not registered
            // if yes, return error packet
            ENTRY *temp, *prev;
            temp = list[i].head;
            prev = NULL;
            while (temp != NULL) {
                if (strcmp(temp->usr, user) == 0) {
                    if (prev == NULL) {
                        fprintf(stderr, "deleted head\n");
                        // case where head node is the one to be deleted
                        list[i].head = list[i].head->next;
                        if (list[i].head == NULL) {
                            j--;
                            strcpy(list[i].name, "");
                            fprintf(stderr, "actually deleted head\n");
                        }
                    } else {
                        prev->next = temp->next;
                        temp = NULL;
                    }
                    send_Ack(s, *addr);
                    break;
                }
                prev = temp;
                temp = temp->next;
            }
            // if (n == 1) {
            //     send_Error(s, *addr);
            //     return;
            // } else {
            //     if (n == 2) {
            //         fprintf(stderr, "head is null\n");

            //     }

            // }
            // otw, removed correctly
        }

        // send_Error(s, *addr);
    }
    max_index = j;
    return;
}

void registration(int s, char *data, struct sockaddr_in *addr) {
    /* Register the content and the server of the content
     */
    // get user name from data packet (first 10 bytes)
    char *res = strtok(data, "|");
    char *user = malloc(10);

    strcpy(user, res);

    //  user = get_Username(data);
    char *contentName = malloc(10);
    res = strtok(NULL, "|");

    // contentName = get_ContentName(data);
    strcpy(contentName, res);
    char *port = malloc(5);
    res = strtok(NULL, "|");
    fprintf(stderr, "before copy bs: %s\n", user);
    fprintf(stderr, "after copy: %s\n", contentName);
    strcpy(port, res);
    fprintf(stderr, "after copy: %s\n", user);
    fprintf(stderr, "after copy: %s\n", contentName);
    struct sockaddr_in *new_addr = malloc(sizeof(struct sockaddr_in));
    new_addr->sin_addr = addr->sin_addr;
    new_addr->sin_port = htons(atoi(port));
    // addr->sin_port = htons((atoi(port)));
    // fprintf(stderr, "\n%d\n", ntohs(addr->sin_port));
    // fprintf(stderr, "%s", contentName);

    int i = 0;
    for (i = 0; i <= max_index; i++) {
        // content name exists in list
        if (!strcmp(list[i].name, contentName)) {
            // look through list to see if user name already registered this
            // if yes, return error packe
            fprintf(stderr, "user: %s\n", user);
            if (add_Entry(list[i].head, user, new_addr)) {
                send_Error(s, *addr);
                return;  // change this to error packet
            } else {
                send_Ack(s, *addr);
            }
            // otw, added correctly
            return;
        }
    }
    if (max_index == MAX_NUM_CON) {
        send_Error(s, *addr);
        return;
    } else {
        // if here, it means this is a new content piece to be added
        strcpy(list[max_index].name, contentName);

        list[max_index].head = create_Entry(user, *new_addr);
        max_index++;
        send_Ack(s, *addr);
    }
    return;
}

///////////////////////////////////Helpers///////////////////////////////////////
// get and send content list for O type packet
void content_List(int s, char *strings[], size_t size,
                  struct sockaddr_in addr) {
    // make sure size >= 2 here, based on your actual logic
    int i, j = 0;
    for (i = 0; i <= max_index; i++) {
        if (list[i].head != NULL) {
            // strings[j] = list[i].name;
            // j++;
            send_Packet(s, 'O', list[i].name, addr);
        }
    }
    send_Packet(s, 'F', "", addr);
}

//////////////////Packet Helpers////////////////////////
// used for creating packet (PDU), TODO
PDU *create_Packet(char type, char *content) {
    PDU *packet = malloc(sizeof(PDU));
    bzero(packet->data, 100);
    packet->type = type;
    strcpy(packet->data, content);
    // bzero(packet->data, 100);
    return packet;
}

// used for sending acknowledgement that operaition was successful
void send_Ack(int s, struct sockaddr_in addr) {
    send_Packet(s, 'A', "Ack", addr);
    return;
}
// used for sending error packet => work on sending through sd after packet
// created
void send_Error(int s, struct sockaddr_in addr) {
    send_Packet(s, 'E', "Error", addr);
    return;
}
char *Addr_to_char(struct sockaddr_in addr) {
    char *address = malloc(sizeof(addr.sin_addr) + sizeof(addr.sin_port));
    bzero(address, 21);
    sprintf(address, "%s:", inet_ntoa(addr.sin_addr));
    char *port = malloc(sizeof(addr.sin_port) + 1);
    bzero(port, 6);
    sprintf(port, "%d", ntohs(addr.sin_port));
    strcat(address, port);
    return address;
}
// used to send packets TODO => work on sending through sd after packet
// created
void send_Packet(int s, char type, char *content, struct sockaddr_in addr) {
    PDU *packet = malloc(sizeof(PDU));
    packet = create_Packet(type, content);
    sendto(s, packet, sizeof(*packet), 0, (const struct sockaddr *)&addr,
           sizeof(addr));
    return;
}
/////////////////////////////////////////////////////////////

/////////////////Entry Struct Helpers////////////////////////

// create linked list node
ENTRY *create_Entry(char *data, struct sockaddr_in addr) {
    ENTRY *user = malloc(sizeof(ENTRY));
    strcpy(user->usr, data);
    user->addr = addr;
    user->token = 0;
    user->next = NULL;
    return user;
}

// search linked list
ENTRY search_Entry(struct entry *head) {
    ENTRY *temp, *res;
    int LRU = INT_MAX;
    if (head == NULL) {
        return *res;
    } else {
        temp = head;
        // if (head->next == NULL) {
        //     head->token++;
        //     return *head;
        // }
        while (temp != NULL) {
            fprintf(stderr, "here in search\n");
            if (temp->token < LRU) {
                LRU = temp->token;
                res = temp;
            }
            temp = temp->next;
        }
    }
    res->token++;
    return *res;
}

// linked list deletion
int delete_Entry(struct entry *head, char *user, struct sockaddr_in *addr) {
    ENTRY *temp, *prev;
    if (head == NULL) {
        return 1;
    } else

    {
        temp = head;
        prev = NULL;
        while (temp != NULL) {
            if (strcmp(temp->usr, user) == 0) {
                if (prev == NULL) {
                    fprintf(stderr, "deleted head\n");
                    // case where head node is the one to be deleted
                    head = head->next;
                    if (head == NULL) {
                        fprintf(stderr, "actually deleted head\n");
                    }
                } else {
                    prev->next = temp->next;
                    temp = NULL;
                }
                return 0;
            }
            prev = temp;
            temp = temp->next;
        }
        return 1;
    }
}
// linkedlist traversal
int add_Entry(struct entry *head, char *user, struct sockaddr_in *addr) {
    ENTRY *newContent, *temp;
    fprintf(stderr, "gsgs %s\n", user);
    newContent = create_Entry(user, *addr);
    if (head == NULL) {
        head = newContent;
    } else {
        temp = head;
        while (temp->next != NULL) {
            if (strcmp(temp->usr, newContent->usr) == 0) {
                return 1;
            }
            temp = temp->next;
        }
        temp->next = newContent;
    }
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
