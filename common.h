#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define TAMANHO_EMISSOR 128
#define TAMANHO_MENSAGEM 512
#define QUANTIDADE_MAXIMA_MENSAGENS 10

typedef struct
{
    char emissor[TAMANHO_EMISSOR];
    char mensagem[TAMANHO_MENSAGEM];
} Mensagem;

typedef struct
{
    int tamanho;
    Mensagem *mensagem;
    int quantidadeMensagens;
} MensagemFila;

typedef struct
{
    int socket;
    struct sockaddr_in endereco;
    struct sockaddr_in6 enderecov6;

    MensagemFila send_buffer;
    Mensagem sending_buffer;
    size_t current_sending_byte;

    Mensagem receiving_buffer;
    size_t current_receiving_byte;
} Usuario;
