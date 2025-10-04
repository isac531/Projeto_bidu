#include "libtslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define MAX_PENDING 20
#define BUFFER_SIZE 2048
#define DEFAULT_PORT 8080

// Estrutura para requisição (recurso compartilhado entre threads)
typedef struct {
    int socket;
    int id;
} request_t;

// Servidor global com recursos compartilhados protegidos por mutex
typedef struct {
    logger_t *logger;
    pthread_mutex_t stats_mutex;
    int total_requests;
    int active_connections;
} server_t;

server_t server;

// Envia resposta HTTP simples (texto puro)
void send_response(int sock, int status, const char *msg) {
    char response[BUFFER_SIZE];
    const char *status_text = (status == 200) ? "OK" : "Not Found";
    
    snprintf(response, sizeof(response),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n%s",
             status, status_text, strlen(msg), msg);
    
    send(sock, response, strlen(response), 0);
}

// Handler de requisições (cada thread executa isso)
void* handle_client(void *arg) {
    request_t *req = (request_t*)arg;
    char buffer[BUFFER_SIZE];
    char method[16], path[256];
    char log_msg[512];
    int bytes;
    
    // Incrementa conexões ativas (seção crítica protegida por mutex)
    pthread_mutex_lock(&server.stats_mutex);
    server.active_connections++;
    pthread_mutex_unlock(&server.stats_mutex);
    
    // Recebe requisição
    bytes = recv(req->socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Erro ao receber dados do cliente #%d: %s", 
                 req->id, strerror(errno));
        tslog_error(server.logger, log_msg);
        goto cleanup;
    }
    
    buffer[bytes] = '\0';
    
    // Parse da requisição HTTP
    if (sscanf(buffer, "%s %s", method, path) != 2) {
        tslog_error(server.logger, "Requisicao HTTP malformada");
        send_response(req->socket, 400, "Bad Request\n");
        goto cleanup;
    }
    
    // Log da requisição (usando logger thread-safe)
    snprintf(log_msg, sizeof(log_msg), 
             "[REQ #%d] %s %s", req->id, method, path);
    tslog_info(server.logger, log_msg);
    
    // Incrementa contador de requisições (seção crítica)
    pthread_mutex_lock(&server.stats_mutex);
    server.total_requests++;
    int total = server.total_requests;
    int active = server.active_connections;
    pthread_mutex_unlock(&server.stats_mutex);
    
    // Processa apenas GET
    if (strcmp(method, "GET") != 0) {
        send_response(req->socket, 404, "Apenas metodo GET suportado\n");
        snprintf(log_msg, sizeof(log_msg), 
                 "[RES #%d] 404 - Metodo %s nao suportado", req->id, method);
        tslog_info(server.logger, log_msg);
        goto cleanup;
    }
    
    // Roteamento simples
    char response_body[1024];
    
    if (strcmp(path, "/") == 0) {
        snprintf(response_body, sizeof(response_body),
                 "=== SERVIDOR WEB ===\n"
                 "Total de requisicoes: %d\n"
                 "Conexoes ativas: %d\n"
                 "\n"
                 "Rotas disponiveis:\n"
                 "  GET /       - Esta pagina\n"
                 "  GET /stats  - Estatisticas JSON\n"
                 "  GET /about  - Sobre o servidor\n",
                 total, active);
        send_response(req->socket, 200, response_body);
        snprintf(log_msg, sizeof(log_msg), "[RES #%d] 200 OK - /", req->id);
        tslog_info(server.logger, log_msg);
    }
    else if (strcmp(path, "/stats") == 0) {
        snprintf(response_body, sizeof(response_body),
                 "{\n"
                 "  \"total_requests\": %d,\n"
                 "  \"active_connections\": %d\n"
                 "}\n",
                 total, active);
        send_response(req->socket, 200, response_body);
        snprintf(log_msg, sizeof(log_msg), "[RES #%d] 200 OK - /stats", req->id);
        tslog_info(server.logger, log_msg);
    }
    else if (strcmp(path, "/about") == 0) {
        snprintf(response_body, sizeof(response_body),
                 "Servidor Web HTTP Simples em C\n"
                 "\n"
                 "Recursos implementados:\n"
                 "  - Threads (pthread) - 1 por conexao\n"
                 "  - Mutex para exclusao mutua\n"
                 "  - Sockets TCP/IP\n"
                 "  - Logging thread-safe (libtslog)\n"
                 "  - Gerenciamento de recursos\n"
                 "  - Tratamento de erros\n");
        send_response(req->socket, 200, response_body);
        snprintf(log_msg, sizeof(log_msg), "[RES #%d] 200 OK - /about", req->id);
        tslog_info(server.logger, log_msg);
    }
    else {
        send_response(req->socket, 404, "404 - Pagina nao encontrada\n");
        snprintf(log_msg, sizeof(log_msg), "[RES #%d] 404 - %s", req->id, path);
        tslog_info(server.logger, log_msg);
    }

cleanup:
    // Decrementa conexões ativas (seção crítica)
    pthread_mutex_lock(&server.stats_mutex);
    server.active_connections--;
    pthread_mutex_unlock(&server.stats_mutex);
    
    // Gerenciamento de recursos: fecha socket e libera memória
    close(req->socket);
    free(req);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    pthread_t thread;
    int request_id = 1;
    char log_msg[256];
    
    // Parse porta
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Porta invalida: %d\n", port);
            return 1;
        }
    }
    
    printf("=== SERVIDOR WEB HTTP ===\n");
    printf("Porta: %d\n", port);
    printf("=========================\n\n");
    
    // Cria arquivo de log se não existir (fix para WSL)
    FILE *test = fopen("web_server.log", "a");
    if (test) fclose(test);
    
    // Inicializa logger thread-safe (obrigatório)
    server.logger = tslog_init("web_server.log");
    if (!server.logger) {
        fprintf(stderr, "ERRO: Falha ao inicializar logger\n");
        perror("Detalhes");
        return 1;
    }
    
    // Inicializa mutex (exclusão mútua - obrigatório)
    if (pthread_mutex_init(&server.stats_mutex, NULL) != 0) {
        tslog_error(server.logger, "Falha ao inicializar mutex");
        tslog_destroy(server.logger);
        return 1;
    }
    
    server.total_requests = 0;
    server.active_connections = 0;
    
    tslog_info(server.logger, "=== Servidor iniciado ===");
    
    // Cria socket TCP/IP (obrigatório)
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Erro ao criar socket: %s", strerror(errno));
        tslog_error(server.logger, log_msg);
        fprintf(stderr, "ERRO: %s\n", log_msg);
        goto cleanup;
    }
    
    // Permite reusar porta
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        tslog_error(server.logger, "Falha em setsockopt");
    }
    
    // Configura endereço
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Erro no bind porta %d: %s", port, strerror(errno));
        tslog_error(server.logger, log_msg);
        fprintf(stderr, "ERRO: %s\n", log_msg);
        close(server_socket);
        goto cleanup;
    }
    
    // Listen
    if (listen(server_socket, MAX_PENDING) < 0) {
        snprintf(log_msg, sizeof(log_msg), 
                 "Erro no listen: %s", strerror(errno));
        tslog_error(server.logger, log_msg);
        fprintf(stderr, "ERRO: %s\n", log_msg);
        close(server_socket);
        goto cleanup;
    }
    
    snprintf(log_msg, sizeof(log_msg), "Escutando na porta %d", port);
    tslog_info(server.logger, log_msg);
    printf("Servidor rodando em http://localhost:%d\n", port);
    printf("Pressione Ctrl+C para encerrar\n\n");
    
    // Loop principal - aceita conexões
    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, 
                              (struct sockaddr*)&client_addr, 
                              &client_len);
        
        if (client_socket < 0) {
            snprintf(log_msg, sizeof(log_msg), 
                     "Erro ao aceitar conexao: %s", strerror(errno));
            tslog_error(server.logger, log_msg);
            continue;
        }
        
        // Aloca estrutura para a thread (gerenciamento de recursos)
        request_t *req = malloc(sizeof(request_t));
        if (!req) {
            tslog_error(server.logger, "Falha ao alocar memoria para requisicao");
            close(client_socket);
            continue;
        }
        
        req->socket = client_socket;
        req->id = request_id++;
        
        // Cria thread (uso de threads - obrigatório)
        if (pthread_create(&thread, NULL, handle_client, req) != 0) {
            snprintf(log_msg, sizeof(log_msg), 
                     "Erro ao criar thread: %s", strerror(errno));
            tslog_error(server.logger, log_msg);
            close(client_socket);
            free(req);
            continue;
        }
        
        // Detach thread (gerenciamento de recursos)
        pthread_detach(thread);
    }
    
cleanup:
    // Limpeza de recursos
    pthread_mutex_destroy(&server.stats_mutex);
    tslog_destroy(server.logger);
    return 0;
}