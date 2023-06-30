#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

#include "common.h"

#define MAXIMO_CONEXOES 15

Usuario listaConexoes[MAXIMO_CONEXOES];
char read_buffer[1024];

int inicializarEnderecoSocket(const char *tipoIp, const char *stringPorta, struct sockaddr_storage *storage)
{
    uint16_t porta = (uint16_t)atoi(stringPorta);

    if (porta == 0)
        return -1;

    porta = htons(porta);
    memset(storage, 0, sizeof(*storage));

    if (strcmp(tipoIp, "v4") == 0)
    {
        struct sockaddr_in *enderecov4 = (struct sockaddr_in *)storage;
        enderecov4->sin_family = AF_INET;
        enderecov4->sin_port = porta;
        enderecov4->sin_addr.s_addr = INADDR_ANY;
        return 1;
    }

    if (strcmp(tipoIp, "v6") == 0)
    {
        struct sockaddr_in6 *enderecov6 = (struct sockaddr_in6 *)storage;
        enderecov6->sin6_family = AF_INET6;
        enderecov6->sin6_port = porta;
        enderecov6->sin6_addr = in6addr_any;
        return 1;
    }

    return 0;
}

int configurarSocket(const char *tipoIp, const char *porta)
{
    struct sockaddr_storage storage;

    if (inicializarEnderecoSocket(tipoIp, porta, &storage) == 0)
    {
        printf("missing arguments.\n");
        exit(1);
    }

    int socketServidor = socket(storage.ss_family, SOCK_STREAM, 0);
    if (socketServidor == -1)
    {
        printf("error creating socket.\n");
        exit(1);
    }

    int habilitado = 1;
    if (setsockopt(socketServidor, SOL_SOCKET, SO_REUSEADDR, &habilitado, sizeof(int)) != 0)
    {
        printf("error setsockopt\n");
        exit(1);
    }

    struct sockaddr *endereco = (struct sockaddr *)(&storage);

    if (bind(socketServidor, endereco, sizeof(storage)) != 0)
    {
        printf("error bind\n");
        exit(1);
    }

    if (listen(socketServidor, 1) != 0)
    {
        printf("error listen\n");
        exit(1);
    }

    return socketServidor;
}

int configurarFdSetLeitura(fd_set *fdSet, int socketServidor)
{
    FD_ZERO(fdSet);
    FD_SET(STDIN_FILENO, fdSet);
    FD_SET(socketServidor, fdSet);

    for (int i = 0; i < MAXIMO_CONEXOES; i++)
    {
        if (listaConexoes[i].socket != -1)
        {
            FD_SET(listaConexoes[i].socket, fdSet);
        }
    }

    return 0;
}

int configurarFdSetEscrita(fd_set *fdSet)
{
    FD_ZERO(fdSet);

    for (int i = 0; i < MAXIMO_CONEXOES; i++)
    {
        if (listaConexoes[i].socket != -1 && listaConexoes[i].send_buffer.quantidadeMensagens > 0)
        {
            FD_SET(listaConexoes[i].socket, fdSet);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("missing arguments.\n");
        exit(1);
    }

    int socketServidor = configurarSocket(argv[1], argv[2]);

    int flag = fcntl(STDIN_FILENO, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flag);

    for (int i = 0; i < MAXIMO_CONEXOES; i++)
    {
        listaConexoes[i].socket = -1;
        listaConexoes[i].send_buffer.mensagem = calloc(QUANTIDADE_MAXIMA_MENSAGENS, sizeof(Mensagem));
        listaConexoes[i].send_buffer.tamanho = QUANTIDADE_MAXIMA_MENSAGENS;
        listaConexoes[i].send_buffer.quantidadeMensagens = 0;
        listaConexoes[i].current_sending_byte = -1;
        listaConexoes[i].current_receiving_byte = 0;
    }

    fd_set fdSetLeitura, fdSetEscrita, fdSetExcecoes;
    int high_sock = socketServidor;

    while (1)
    {
        configurarFdSetLeitura(&fdSetLeitura, socketServidor);
        configurarFdSetEscrita(&fdSetEscrita);
        configurarFdSetLeitura(&fdSetExcecoes, socketServidor);

        high_sock = socketServidor;

        for (int i = 0; i < MAXIMO_CONEXOES; i++)
        {
            if (listaConexoes[i].socket > high_sock)
            {
                high_sock = listaConexoes[i].socket;
            }
        }

        int activity = select(high_sock + 1, &fdSetLeitura, &fdSetEscrita, &fdSetExcecoes, NULL);
    }

    return 0;
}