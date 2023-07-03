#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 500
#define MAX_CONECTIONS 15

int sockfd;
int broadcastfd;
int numberThreads = 0;
int numberEquipments = 0;
struct sockaddr_in broadcast_addr;

enum COMMAND_TYPE
{
    REQ_ADD = 1,
    REQ_REM = 2,
    RES_ADD = 3,
    RES_LIST = 4,
    REQ_INF = 5,
    RES_INF = 6,
    ERROR = 7,
    OK = 8,
};

enum ERRORS_TYPE
{
    EQ_NOT_FOUND = 1,
    SOURCE_EQ_NOT_FOUND = 2,
    TARGET_EQ_NOT_FOUND = 3,
    LIMIT_EXCEED = 4,
};

typedef struct
{
    int idMessage;
    int idOrigem;
    int idDestino;
    char *conteudo;
} Command;

typedef struct
{
    int id;
    struct sockaddr_in adresses;
} Users;

Users avaiableEquipments[MAX_CONECTIONS] = {0};

struct ThreadArgs
{
    socklen_t clientAdressSize;
    struct sockaddr_in clientAdress;
    char buffer[MAX_MESSAGE_SIZE];
};

void inicializeAvaiableEquipments()
{
    for (int i = 0; i < MAX_CONECTIONS; i++)
    {
        avaiableEquipments[i].id = -1;
    }
}

void *ThreadMain(void *threadArgs);

int initializeServerSocket(const char *port, struct sockaddr **address)
{
    int domain, addressSize, serverfd, yes = 1;
    struct sockaddr_in addressv4;

    addressv4.sin_family = AF_INET;
    addressv4.sin_port = htons(atoi(port));
    addressv4.sin_addr.s_addr = htonl(INADDR_ANY);

    domain = AF_INET;
    addressSize = sizeof(addressv4);
    *address = (struct sockaddr *)&addressv4;

    if ((serverfd = socket(domain, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(yes)) == -1)
    {
        perror("Could not set socket option");
        exit(EXIT_FAILURE);
    }

    if (bind(serverfd, *address, addressSize) < 0)
    {
        perror("Could not bind port to socket");
        exit(EXIT_FAILURE);
    }

    return serverfd;
}

int initializeBroadcastServerSocket(const char *port)
{
    int domain, addressSize, serverfd, yes = 1;
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(port));
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    domain = AF_INET;
    addressSize = sizeof(address);
    struct sockaddr *addr = (struct sockaddr *)&address;

    if ((serverfd = socket(domain, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR | SO_BROADCAST, &yes, sizeof(yes)) == -1)
    {
        perror("Could not set socket option");
        exit(EXIT_FAILURE);
    }

    if (bind(serverfd, addr, addressSize) < 0)
    {
        perror("Could not bind port to socket");
        exit(EXIT_FAILURE);
    }

    return serverfd;
}

int returnEmptyArrayIndex()
{
    for (int i = 0; i < MAX_CONECTIONS; i++)
    {
        if (avaiableEquipments[i].id == -1)
        {
            return i;
        }
    }
    return -1;
}

void mountAddResponse(char *buffer, int i)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d\n", RES_ADD, i);
    strcat(buffer, aux);
}

void mountRemoveResponse(char *buffer, Command command)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d %d\n", OK, command.idOrigem, 1);
    strcat(buffer, aux);
}

void mountRemoveBroadcast(char *buffer, Command command)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d\n", REQ_REM, command.idOrigem);
    strcat(buffer, aux);
}

void mountListResponse(char *buffer)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d", RES_LIST);
    for (int i = 0; i < numberEquipments; i++)
    {
        sprintf(aux, "%s %d", aux, avaiableEquipments[i].id);
    }
    sprintf(aux, "%s ", aux);
    sprintf(aux, "%s\n", aux);
    strcat(buffer, aux);
}

void mountErrorResponse(char *buffer, int errorCode)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d\n", ERROR, errorCode);
    strcat(buffer, aux);
}

void addEquipment(char *responseMessage, struct sockaddr_in clientAdress)
{
    if (numberEquipments == MAX_CONECTIONS)
    {
        printf("Limit exceeded\n");
        char buf[MAX_MESSAGE_SIZE] = "";
        mountErrorResponse(buf, LIMIT_EXCEED);
        int byteSend = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientAdress, 16);
        if (byteSend < 1)
        {
            exit(EXIT_FAILURE);
        }
        return;
    }
    int i = returnEmptyArrayIndex();
    avaiableEquipments[i].id = (i + 1);
    avaiableEquipments[i].adresses = clientAdress;
    mountAddResponse(responseMessage, (i + 1));
    numberEquipments++;
    int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);
    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }
    byteSent = sendto(broadcastfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&broadcast_addr, 16);
    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }
    memset(responseMessage, 0, MAX_MESSAGE_SIZE);
    mountListResponse(responseMessage);
    byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);
    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }
    printf("User %d joined the group!\n", i + 1);
};

void removeEquipment(char *responseMessage, struct sockaddr_in idOriginAdress, Command command)
{
    struct sockaddr_in clientAdress;
    if (avaiableEquipments[(command.idOrigem - 1)].id != -1)
    {
        clientAdress = avaiableEquipments[(command.idOrigem - 1)].adresses;
        avaiableEquipments[(command.idOrigem - 1)].id = -1;
        numberEquipments--;
        mountRemoveResponse(responseMessage, command);
        int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);
        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
        printf("User %d removed\n", command.idOrigem);
        memset(responseMessage, 0, MAX_MESSAGE_SIZE);
        mountRemoveBroadcast(responseMessage, command);
        byteSent = sendto(broadcastfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&broadcast_addr, 16);
        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        clientAdress = idOriginAdress;
        mountErrorResponse(responseMessage, EQ_NOT_FOUND);
        int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);
        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
    }
};

void infoEquipment(char *responseMessage, struct sockaddr_in clientAdress, Command command, char *buffer)
{
    printf("entrou acá");
    int foundOrigin = avaiableEquipments[(command.idOrigem - 1)].id == command.idOrigem;
    int foundDestiny = avaiableEquipments[(command.idDestino - 1)].id == command.idDestino;

    if (!foundOrigin)
    {
        printf("User %d not found\n", command.idOrigem);
        mountErrorResponse(responseMessage, SOURCE_EQ_NOT_FOUND);
        int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);
        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
    }
    else if (!foundDestiny)
    {
        printf("User %d not found\n", command.idDestino);
        mountErrorResponse(responseMessage, TARGET_EQ_NOT_FOUND);
        int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&avaiableEquipments[(command.idOrigem - 1)].adresses, 16);
        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        int byteSent = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&avaiableEquipments[(command.idDestino - 1)].adresses, 16);
        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
    }
};

void interpretCommand(struct ThreadArgs *args)
{
    char aux[MAX_MESSAGE_SIZE];
    strcpy(aux, args->buffer);
    char *commandToken = strtok(aux, " ");
    int commandSent = atoi(commandToken);
    char responseMessage[MAX_MESSAGE_SIZE] = "";
    char *mensagem;
    Command command;

    switch (commandSent)
    {
    case REQ_ADD:
        command.idMessage = REQ_ADD;
        addEquipment(responseMessage, args->clientAdress);
        break;
    case REQ_REM:
        command.idMessage = REQ_REM;
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        removeEquipment(responseMessage, args->clientAdress, command);
        break;
    case REQ_INF:
    case RES_INF:
        command.idMessage = commandSent;
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        commandToken = strtok(NULL, " ");
        command.idDestino = atoi(commandToken);
        mensagem = strtok(NULL, " ");
        infoEquipment(responseMessage, args->clientAdress, command, args->buffer);
        break;
    default:
        break;
    }
}

void *ThreadMain(void *threadArgs)
{
    struct ThreadArgs *args = (struct ThreadArgs *)threadArgs;
    interpretCommand(args);
    numberThreads--;
    free(args);
    return NULL;
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        return 0;
    }
    struct sockaddr *address;
    sockfd = initializeServerSocket(argv[1], &address);
    broadcastfd = initializeBroadcastServerSocket("0");
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcast_addr.sin_port = htons(1313);
    inicializeAvaiableEquipments();
    pthread_t threads[MAX_CONECTIONS];

    while (1)
    {
        struct ThreadArgs *args = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
        args->clientAdressSize = sizeof(struct sockaddr_in);
        int byteReceived = recvfrom(sockfd, args->buffer, MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&args->clientAdress, &args->clientAdressSize);
        if (byteReceived < 0)
        {
            perror("Could not receive message from client");
            exit(EXIT_FAILURE);
        }
        int threadResponse = pthread_create(&threads[numberThreads], NULL, ThreadMain, (void *)args);
        if (threadResponse)
        {
            printf("Error creating thread\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            numberThreads++;
        }
    }
    return 0;
}