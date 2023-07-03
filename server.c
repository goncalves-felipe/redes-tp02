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
#define MAX_CONECTIONS 16

int sockfd;
int broadcastfd;
int numberThreads = 0;
int numberUsers = 0;
struct sockaddr_in broadcast_addr;

enum COMMAND_TYPE
{
    REQ_ADD = 1,
    REQ_REM = 2,
    RES_ADD = 3,
    RES_LIST = 4,
    MSG = 6,
    ERROR = 7,
    OK = 8,
};

enum ERRORS_TYPE
{
    LIMIT_EXCEED = 1,
    USER_NOT_FOUND = 2,
    RECEIVER_NOT_FOUND = 3,
};

typedef struct
{
    int idMsg;
    int idSender;
    int idReceiver;
    char *message;
} Command;

typedef struct
{
    int id;
    struct sockaddr_in adresses;
} Users;

Users avaiableUsers[MAX_CONECTIONS] = {0};

struct ThreadArgs
{
    socklen_t clientAdressSize;
    struct sockaddr_in clientAdress;
    char buffer[MAX_MESSAGE_SIZE];
};

void inicializarUsuarios()
{
    for (int i = 0; i < MAX_CONECTIONS; i++)
        avaiableUsers[i].id = -1;
}

void *ThreadMain(void *threadArgs);

int inicializarSocket(const char *porta, struct sockaddr **endereco)
{
    int domain, addressSize, serverfd, yes = 1;
    struct sockaddr_in addressv4;

    addressv4.sin_family = AF_INET;
    addressv4.sin_port = htons(atoi(porta));
    addressv4.sin_addr.s_addr = htonl(INADDR_ANY);

    domain = AF_INET;
    addressSize = sizeof(addressv4);
    *endereco = (struct sockaddr *)&addressv4;

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

    if (bind(serverfd, *endereco, addressSize) < 0)
    {
        perror("Could not bind port to socket");
        exit(EXIT_FAILURE);
    }

    return serverfd;
}

int inicializarSocketBroadcast(const char *porta)
{
    int domain, addressSize, serverfd, yes = 1;
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(porta));
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    domain = AF_INET;
    addressSize = sizeof(address);
    struct sockaddr *endereco = (struct sockaddr *)&address;

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

    if (bind(serverfd, endereco, addressSize) < 0)
    {
        perror("Could not bind port to socket");
        exit(EXIT_FAILURE);
    }

    return serverfd;
}

int retornarPosicaoDisponivelArray()
{
    for (int i = 0; i < MAX_CONECTIONS; i++)
        if (avaiableUsers[i].id == -1)
            return i;

    return -1;
}

void montarRespostaAdd(char *buffer, int i)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d\n", RES_ADD, i);
    strcat(buffer, aux);
}

void montarRespostaRem(char *buffer, Command command)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d %d\n", OK, command.idSender, 1);
    strcat(buffer, aux);
}

void montarRespostaRemBroadcast(char *buffer, Command command)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d\n", REQ_REM, command.idSender);
    strcat(buffer, aux);
}

void montarMensagemBroadcast(char *buffer, Command command)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d %s", MSG, command.idSender, command.message);
    strcat(buffer, aux);
}

void montarRespostaLista(char *buffer)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d", RES_LIST);

    for (int i = 0; i < numberUsers; i++)
    {
        sprintf(aux, "%s %d", aux, avaiableUsers[i].id);
    }

    sprintf(aux, "%s ", aux);
    sprintf(aux, "%s\n", aux);
    strcat(buffer, aux);
}

void montarRespostaErro(char *buffer, int errorCode)
{
    char aux[MAX_MESSAGE_SIZE] = "";
    sprintf(aux, "%d %d\n", ERROR, errorCode);
    strcat(buffer, aux);
}

void adicionarUsuario(char *responseMessage, struct sockaddr_in clientAdress)
{
    if (numberUsers == MAX_CONECTIONS - 1)
    {
        printf("Limit exceeded\n");
        char buf[MAX_MESSAGE_SIZE] = "";

        montarRespostaErro(buf, LIMIT_EXCEED);

        int byteSend = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&clientAdress, 16);

        if (byteSend < 1)
        {
            exit(EXIT_FAILURE);
        }

        return;
    }

    int i = retornarPosicaoDisponivelArray();

    avaiableUsers[i].id = (i + 1);
    avaiableUsers[i].adresses = clientAdress;

    montarRespostaAdd(responseMessage, (i + 1));
    numberUsers++;

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
    montarRespostaLista(responseMessage);

    byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);

    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }

    printf("User %02d added\n", i + 1);
};

void removerUsuario(char *responseMessage, struct sockaddr_in idOriginAdress, Command command)
{
    struct sockaddr_in clientAdress;

    if (avaiableUsers[(command.idSender - 1)].id != -1)
    {
        clientAdress = avaiableUsers[(command.idSender - 1)].adresses;
        avaiableUsers[(command.idSender - 1)].id = -1;

        numberUsers--;

        montarRespostaRem(responseMessage, command);

        int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);

        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }

        printf("User %02d removed\n", command.idSender);

        memset(responseMessage, 0, MAX_MESSAGE_SIZE);
        montarRespostaRemBroadcast(responseMessage, command);

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
        montarRespostaErro(responseMessage, USER_NOT_FOUND);

        int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&clientAdress, 16);

        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }
    }
};

void enviarMensagem(char *responseMessage, Command command, char *buffer)
{
    if (command.idReceiver == -2)
    {
        time_t now = time(NULL);
        struct tm *tm_struct = localtime(&now);

        int hour = tm_struct->tm_hour;
        int minutes = tm_struct->tm_min;

        memset(buffer, 0, MAX_MESSAGE_SIZE);
        montarMensagemBroadcast(buffer, command);

        int byteSent = sendto(broadcastfd, buffer, strlen(buffer), 0, (struct sockaddr *)&broadcast_addr, 16);

        if (byteSent < 1)
        {
            perror("Could not send message");
            exit(EXIT_FAILURE);
        }

        printf("[%02d:%02d] %02d: %s", hour, minutes, command.idSender, command.message);
    }
    else
    {
        int foundDestiny = avaiableUsers[(command.idReceiver - 1)].id == command.idReceiver;

        if (!foundDestiny)
        {
            printf("User %02d not found\n", command.idReceiver);

            montarRespostaErro(responseMessage, RECEIVER_NOT_FOUND);

            int byteSent = sendto(sockfd, responseMessage, strlen(responseMessage), 0, (struct sockaddr *)&avaiableUsers[(command.idSender - 1)].adresses, 16);

            if (byteSent < 1)
            {
                perror("Could not send message");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            int byteSent = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&avaiableUsers[(command.idReceiver - 1)].adresses, 16);
            if (byteSent < 1)
            {
                perror("Could not send message");
                exit(EXIT_FAILURE);
            }
        }
    }
};

void tratarComando(struct ThreadArgs *args)
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
        command.idMsg = REQ_ADD;
        adicionarUsuario(responseMessage, args->clientAdress);
        break;

    case REQ_REM:
        command.idMsg = REQ_REM;
        commandToken = strtok(NULL, " ");
        command.idSender = atoi(commandToken);
        removerUsuario(responseMessage, args->clientAdress, command);
        break;

    case MSG:
        command.idMsg = commandSent;

        commandToken = strtok(NULL, " ");
        command.idSender = atoi(commandToken);

        commandToken = strtok(NULL, " ");
        command.idReceiver = atoi(commandToken);

        mensagem = strtok(NULL, "");
        command.message = mensagem;

        enviarMensagem(responseMessage, command, args->buffer);
        break;

    default:
        break;
    }
}

void *ThreadMain(void *threadArgs)
{
    struct ThreadArgs *args = (struct ThreadArgs *)threadArgs;
    tratarComando(args);

    numberThreads--;

    free(args);
    return NULL;
}

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {
        return 0;
    }

    struct sockaddr *address;

    sockfd = inicializarSocket(argv[2], &address);
    broadcastfd = inicializarSocketBroadcast("0");

    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcast_addr.sin_port = htons(1313);

    inicializarUsuarios();
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