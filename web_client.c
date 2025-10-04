#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define BUFFER_SIZE 2048
#define DEFAULT_PORT 8080

int make_request(const char *host, int port, const char *path) {
    int sock;
    struct sockaddr_in server_addr;
    char request[512];
    char response[BUFFER_SIZE];
    int bytes;
    
    // Cria socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "ERRO: Falha ao criar socket: %s\n", strerror(errno));
        return -1;
    }
    
    // Configura endereço
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "ERRO: Endereco invalido: %s\n", host);
        close(sock);
        return -1;
    }
    
    // Conecta
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "ERRO: Falha ao conectar em %s:%d - %s\n", 
                host, port, strerror(errno));
        close(sock);
        return -1;
    }
    
    printf("Conectado a %s:%d\n", host, port);
    
    // Monta requisição HTTP GET
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host, port);
    
    // Envia requisição
    if (send(sock, request, strlen(request), 0) < 0) {
        fprintf(stderr, "ERRO: Falha ao enviar requisicao\n");
        close(sock);
        return -1;
    }
    
    printf("\n>>> Requisicao enviada:\nGET %s\n\n", path);
    
    // Recebe resposta
    printf("<<< Resposta:\n");
    printf("----------------------------------------\n");
    while ((bytes = recv(sock, response, BUFFER_SIZE - 1, 0)) > 0) {
        response[bytes] = '\0';
        printf("%s", response);
    }
    printf("----------------------------------------\n\n");
    
    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    char *host = "127.0.0.1";
    int port = DEFAULT_PORT;
    char *path = "/";
    
    printf("=== CLIENTE WEB HTTP ===\n\n");
    
    // Parse argumentos
    if (argc >= 2) path = argv[1];
    if (argc >= 3) host = argv[2];
    if (argc >= 4) port = atoi(argv[3]);
    
    printf("Host: %s\n", host);
    printf("Porta: %d\n", port);
    printf("Path: %s\n", path);
    printf("========================\n\n");
    
    if (make_request(host, port, path) < 0) {
        return 1;
    }
    
    return 0;
}