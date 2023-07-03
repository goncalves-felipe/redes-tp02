// Client side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define STDIN 0
#define MAX_MESSAGE_SIZE 500
#define NUMBER_USERS 15
#define MAX_CONECTIONS 15

enum COMMAND_TYPE
{
    REQ_ADD = 1,
    REQ_REM = 2,
    RES_ADD = 3,
    RES_LIST = 4,
    MSG = 5,
    ERROR = 7,
    OK = 8,
};

enum ERRORS_TYPE
{
    EQ_NOT_FOUND = 1,
    TARGET_EQ_NOT_FOUND = 2,
    LIMIT_EXCEED = 3,
};

int userId = 1;
int users[NUMBER_USERS] = {0};
int clientfd;
int broadcastfd;
socklen_t clientAdressSize = sizeof(struct sockaddr_in);
struct ThreadArgs
{
    struct sockaddr_in serverAdress;
};

typedef struct
{
    int idMessage;
    int idOrigem;
    int idDestino;
    char *conteudo;
} Command;

void requestAdd(struct sockaddr_in serverAdress)
{
    char message[MAX_MESSAGE_SIZE];
    Command command;
    command.idMessage = REQ_ADD;
    sprintf(message, "%d", command.idMessage);
    int byteSent = sendto(clientfd, message, strlen(message), 0,
                          (struct sockaddr *)&serverAdress, 16);
    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }
}

void requestRemove(struct sockaddr_in serverAdress)
{
    char message[MAX_MESSAGE_SIZE];
    Command command;
    command.idMessage = REQ_REM;
    command.idOrigem = userId;
    sprintf(message, "%d %d", command.idMessage, command.idOrigem);
    int byteSent = sendto(clientfd, message, strlen(message), 0,
                          (struct sockaddr *)&serverAdress, 16);
    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }
}

void sendMessage(int targetId, struct sockaddr_in serverAdress, char *mensagem)
{
    char message[MAX_MESSAGE_SIZE] = "";
    Command command;

    command.idMessage = MSG;
    command.idOrigem = userId;
    command.idDestino = targetId;
    command.conteudo = mensagem;

    sprintf(message, "%d %d %d %s", command.idMessage, command.idOrigem, command.idDestino, command.conteudo);
    int byteSent = sendto(clientfd, message, strlen(message), 0, (struct sockaddr *)&serverAdress, 16);
    if (byteSent < 1)
    {
        perror("Could not send message");
        exit(EXIT_FAILURE);
    }
}

void listUsers()
{
    for (int i = 0; i < MAX_CONECTIONS; i++)
    {
        if (users[i] != 0 && users[i] != userId)
        {
            printf("%02d ", users[i]);
        }
    }
    printf("\n");
}

void executeCommand(char *command, struct sockaddr_in serverAdress)
{
    char *buffer = strdup(command);
    char *commandToken;
    char *mensagem;
    char symbol = command[0];
    int destinoID;

    switch (symbol)
    {
    case 'l':
        listUsers();
        break;
    case 'c':
        requestRemove(serverAdress);
        break;
    case 's':
        strtok(buffer, " ");        // send
        buffer = strtok(NULL, " "); // to/all
        if (strcmp(buffer, "to") == 0)
        {
            commandToken = strtok(NULL, " ");
            destinoID = atoi(commandToken);
            mensagem = strtok(NULL, "");
            sendMessage(destinoID, serverAdress, mensagem);
        }
        else if (strcmp(buffer, "all") == 0)
        {
            mensagem = strtok(NULL, "");
            sendMessage(-2, serverAdress, mensagem);
        }
        break;
    default:
        break;
    }
}

int asyncRead(char *message)
{
    struct timeval tv;
    fd_set readfds;
    tv.tv_sec = 0;
    tv.tv_usec = 60000;
    FD_ZERO(&readfds);
    FD_SET(1, &readfds);
    select(1 + 1, &readfds, NULL, NULL, &tv);
    if (FD_ISSET(1, &readfds))
    {
        read(1, message, MAX_MESSAGE_SIZE - 1);
        return 1;
    }
    else
        fflush(stdout);
    return 0;
}

void executeErrorID(int errorCode)
{
    switch (errorCode)
    {
    case EQ_NOT_FOUND:
        printf("User not found\n");
        break;
    case TARGET_EQ_NOT_FOUND:
        printf("Receiver not found\n");
        break;
    case LIMIT_EXCEED:
        printf("User limit exceeded\n");
        exit(-1);
        break;
    }
}
void executeRESADD(int id)
{
    userId = id;
}

void executeBroadcastRESADD(int id)
{
    printf("User %02d joined the group!\n", id);
    for (int i = 0; i < MAX_CONECTIONS; i++)
    {
        if (users[i] == id)
        {
            break;
        }
        if (users[i] == 0)
        {
            users[i] = id;
            break;
        }
    }
}
void executeBroadcastREQREM(int id)
{
    if (id != userId)
    {
        printf("User %02d left the group!\n", id);
    }

    for (int i = 0; i < MAX_CONECTIONS; i++)
    {
        if (users[i] == id)
        {
            users[i] = 0;
            break;
        }
    }
}

void executeBroadcastREQRES(int id, char *mensagem)
{
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);

    int hour = tm_struct->tm_hour;
    int minutes = tm_struct->tm_min;

    if (id == userId)
    {
        printf("[%02d:%02d] -> all: %s", hour, minutes, mensagem);
    }
    else
    {
        printf("[%02d:%02d] %02d: %s", hour, minutes, id, mensagem);
    }
}

void executeREQRES(int id, char *mensagem)
{
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);

    int hour = tm_struct->tm_hour;
    int minutes = tm_struct->tm_min;
    printf("[%02d:%02d] %02d: %s", hour, minutes, id, mensagem);
}

void executeRESLIST()
{
    char *eq;
    int i = 0;
    while ((eq = strtok(NULL, " ")) != NULL)
    {
        users[i] = atoi(eq);
        i++;
    }
}
void executeOK()
{
    printf("Removed Successfully\n");
    exit(-1);
}

void InterpretCommand(char *buffer)
{
    char *commandToken = strtok(buffer, " ");
    int commandReceive = atoi(commandToken);
    Command command;
    char *mensagem;

    switch (commandReceive)
    {
    case ERROR:
        commandToken = strtok(NULL, " ");
        int errorCode = atoi(commandToken);
        executeErrorID(errorCode);
        break;
    case RES_ADD:
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        executeRESADD(command.idOrigem);
        break;
    case RES_LIST:
        executeRESLIST();
        break;
    case OK:
        executeOK();
        break;
    case MSG:
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        commandToken = strtok(NULL, " ");
        command.idDestino = atoi(commandToken);
        mensagem = strtok(NULL, "");
        command.conteudo = mensagem;
        executeREQRES(command.idOrigem, command.conteudo);
        break;
    }
}
void InterpretBroadcastCommand(char *buffer)
{
    char *commandToken = strtok(buffer, " ");
    int commandReceive = atoi(commandToken);
    Command command;
    char *mensagem;

    switch (commandReceive)
    {
    case RES_ADD:
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        executeBroadcastRESADD(command.idOrigem);
        break;
    case REQ_REM:
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        executeBroadcastREQREM(command.idOrigem);
        break;
    case MSG:
        commandToken = strtok(NULL, " ");
        command.idOrigem = atoi(commandToken);
        mensagem = strtok(NULL, "");
        command.conteudo = mensagem;
        executeBroadcastREQRES(command.idOrigem, command.conteudo);
        break;
    }
}
void *ReceiveMessageThread(void *data)
{
    struct ThreadArgs *threadData = (struct ThreadArgs *)data;
    while (1)
    {
        char buffer[MAX_MESSAGE_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int byteReceived = recvfrom(clientfd, buffer, MAX_MESSAGE_SIZE, 0,
                                    (struct sockaddr *)&threadData->serverAdress,
                                    &clientAdressSize);
        if (byteReceived < 0)
        {
            exit(EXIT_FAILURE);
        }
        buffer[byteReceived] = '\0';
        InterpretCommand(buffer);
    }
    free(threadData);
    pthread_exit(NULL);
}
void *ReceiveBroadThread(void *data)
{
    struct ThreadArgs *threadData = (struct ThreadArgs *)data;
    while (1)
    {
        char buffer[MAX_MESSAGE_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int byteReceived = recvfrom(broadcastfd, buffer, MAX_MESSAGE_SIZE, 0,
                                    (struct sockaddr *)&threadData->serverAdress,
                                    &clientAdressSize);
        if (byteReceived < 0)
        {
            exit(EXIT_FAILURE);
        }
        buffer[byteReceived] = '\0';
        InterpretBroadcastCommand(buffer);
    }
    free(threadData);
    pthread_exit(NULL);
}
void *SendThread(void *data)
{
    struct ThreadArgs *threadData = (struct ThreadArgs *)data;
    while (1)
    {
        char buffer[MAX_MESSAGE_SIZE];
        memset(buffer, 0, sizeof(buffer));
        if (asyncRead(buffer))
        {
            executeCommand(strdup(buffer), threadData->serverAdress);
        }
    }
    free(threadData);
    pthread_exit(NULL);
}
int initializeClientSocket(const char *port)
{
    int addressSize, clientfd, yes = 1;
    struct sockaddr *address;
    struct sockaddr_in addressv4;

    addressv4.sin_addr.s_addr = htonl(INADDR_ANY);
    addressv4.sin_port = htons(atoi(port));
    addressv4.sin_family = AF_INET;

    addressSize = sizeof(addressv4);
    address = (struct sockaddr *)&addressv4;

    if ((clientfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes,
                   sizeof(yes)) == -1)
    {
        perror("Could not set socket option");
        exit(EXIT_FAILURE);
    }
    if (bind(clientfd, address, addressSize) < 0)
    {
        perror("Could not bind port to socket");
        exit(EXIT_FAILURE);
    }
    return clientfd;
}

int main(int argc, char const *argv[])
{
    srand(time(0));
    if (argc != 3)
    {
        return 0;
    }

    clientfd = initializeClientSocket("0");
    broadcastfd = initializeClientSocket("1313");
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[2]));
    serverAddress.sin_addr.s_addr = inet_addr(argv[1]);

    struct sockaddr_in broadcastAddress;
    broadcastAddress.sin_family = AF_INET;
    broadcastAddress.sin_port = htons(1313);
    broadcastAddress.sin_addr.s_addr = inet_addr(argv[1]);

    requestAdd(serverAddress);

    pthread_t receiveThread;
    struct ThreadArgs *args =
        (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
    args->serverAdress = serverAddress;
    int byteReceived = pthread_create(&receiveThread, NULL, ReceiveMessageThread, args);
    if (byteReceived != 0)
    {
        printf("Error creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_t sendThread;
    struct ThreadArgs *sendThreadArgs =
        (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
    sendThreadArgs->serverAdress = serverAddress;
    int byteStatus =
        pthread_create(&sendThread, NULL, SendThread, sendThreadArgs);
    if (byteStatus != 0)
    {
        printf("Error creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_t broadcastThread;
    struct ThreadArgs *broadcastThreadArgs =
        (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
    broadcastThreadArgs->serverAdress = broadcastAddress;
    int byteBroadcast = pthread_create(&broadcastThread, NULL, ReceiveBroadThread, args);
    if (byteBroadcast != 0)
    {
        printf("Error creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_join(receiveThread, NULL);
    pthread_join(sendThread, NULL);
    pthread_join(broadcastThread, NULL);
    return 0;
}