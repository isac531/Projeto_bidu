#include "libtslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define MAX_PENDING 20
#define BUFFER_SIZE 4096
#define DEFAULT_PORT 8080
#define THREAD_POOL_SIZE 10
#define MAX_QUEUE_SIZE 100

typedef struct {
    int socket;
    int id;
} request_t;

typedef struct {
    request_t *queue[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} work_queue_t;

typedef struct {
    logger_t *logger;
    pthread_mutex_t stats_mutex;
    int total_requests;
    int request_id;
    work_queue_t *work_queue;
    pthread_t thread_pool[THREAD_POOL_SIZE];
} server_t;

server_t server;
volatile sig_atomic_t g_running = 1;

void handle_sigint(int signum) {
    g_running = 0;
}

// ======================== WORK QUEUE ========================

work_queue_t* work_queue_init() {
    work_queue_t *queue = malloc(sizeof(work_queue_t));
    if (!queue) return NULL;
    
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    
    return queue;
}

void work_queue_destroy(work_queue_t *queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Libera requisições pendentes
    while (queue->count > 0) {
        request_t *req = queue->queue[queue->front];
        close(req->socket);
        free(req);
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
        queue->count--;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

int work_queue_push(work_queue_t *queue, request_t *req) {
    pthread_mutex_lock(&queue->mutex);
    
    // Aguarda espaço na fila (com timeout para não bloquear indefinidamente)
    while (queue->count >= MAX_QUEUE_SIZE && g_running) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // timeout de 1 segundo
        
        int result = pthread_cond_timedwait(&queue->not_full, &queue->mutex, &timeout);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&queue->mutex);
            return -1; // Fila cheia, rejeita conexão
        }
    }
    
    if (!g_running) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    queue->queue[queue->rear] = req;
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

request_t* work_queue_pop(work_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);
    
    // Aguarda trabalho na fila
    while (queue->count == 0 && g_running) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    if (!g_running && queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    request_t *req = queue->queue[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->count--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return req;
}

// ======================== HTTP FUNCTIONS ========================

// Retorna o tipo de conteúdo (MIME type) com base na extensão do arquivo.
const char* get_mime_type(const char* path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    if (strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".txt") == 0) return "text/plain";
    return "application/octet-stream";
}

// Lê um arquivo do disco e o carrega na memória.
char* read_file(const char* path, long* file_size) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(*file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, *file_size, file);
    buffer[*file_size] = '\0';
    fclose(file);
    return buffer;
}

// Envia uma resposta HTTP completa para o cliente.
void send_http_response(int sock, const char* status, const char* mime_type, const char* content, long content_length) {
    char header_buffer[BUFFER_SIZE];
    snprintf(header_buffer, sizeof(header_buffer),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, mime_type, content_length);

    send(sock, header_buffer, strlen(header_buffer), 0);
    send(sock, content, content_length, 0);
}

// Lida com a conexão de um único cliente.
void handle_client_request(request_t *req) {
    char buffer[BUFFER_SIZE];
    char method[16], path[256], version[16];
    char log_msg[512];

    ssize_t bytes = recv(req->socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        goto cleanup;
    }
    buffer[bytes] = '\0';

    if (sscanf(buffer, "%15s %255s %15s", method, path, version) != 3) {
        send_http_response(req->socket, "400 Bad Request", "text/plain", "Bad Request", 11);
        goto cleanup;
    }

    pthread_mutex_lock(&server.stats_mutex);
    server.total_requests++;
    pthread_mutex_unlock(&server.stats_mutex);
    
    snprintf(log_msg, sizeof(log_msg), "[REQ #%d] %s %s", req->id, method, path);
    tslog_info(server.logger, log_msg);

    if (strcmp(method, "GET") != 0) {
        send_http_response(req->socket, "405 Method Not Allowed", "text/plain", "Method Not Allowed", 18);
        goto cleanup;
    }
    
    char file_path[512];
    if (strcmp(path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "www/index.html");
    } else {
        snprintf(file_path, sizeof(file_path), "www%s", path);
    }

    struct stat st;
    if (stat(file_path, &st) != 0) {
        const char* msg = "404 Not Found";
        send_http_response(req->socket, "404 Not Found", "text/plain", msg, strlen(msg));
        snprintf(log_msg, sizeof(log_msg), "[RES #%d] 404 Not Found - %s", req->id, file_path);
        tslog_info(server.logger, log_msg);
    } else {
        long file_size;
        char* file_content = read_file(file_path, &file_size);

        if (file_content) {
            const char* mime_type = get_mime_type(file_path);
            send_http_response(req->socket, "200 OK", mime_type, file_content, file_size);
            snprintf(log_msg, sizeof(log_msg), "[RES #%d] 200 OK - %s", req->id, file_path);
            tslog_info(server.logger, log_msg);
            free(file_content);
        } else {
            const char* msg = "500 Internal Server Error";
            send_http_response(req->socket, "500 Internal Server Error", "text/plain", msg, strlen(msg));
            snprintf(log_msg, sizeof(log_msg), "[RES #%d] 500 Server Error - %s", req->id, file_path);
            tslog_error(server.logger, log_msg);
        }
    }

cleanup:
    close(req->socket);
    free(req);
}

// Thread worker do pool
void* worker_thread(void *arg) {
    char log_msg[256];
    int thread_id = *((int*)arg);
    
    snprintf(log_msg, sizeof(log_msg), "Worker thread #%d iniciada", thread_id);
    tslog_info(server.logger, log_msg);
    
    while (g_running) {
        request_t *req = work_queue_pop(server.work_queue);
        if (req) {
            handle_client_request(req);
        }
    }
    
    snprintf(log_msg, sizeof(log_msg), "Worker thread #%d finalizando", thread_id);
    tslog_info(server.logger, log_msg);
    
    return NULL;
}

// Função para obter próximo request_id de forma thread-safe
int get_next_request_id() {
    pthread_mutex_lock(&server.stats_mutex);
    int id = ++server.request_id;
    pthread_mutex_unlock(&server.stats_mutex);
    return id;
}

// Função principal que inicializa e executa o servidor.
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc >= 2) port = atoi(argv[1]);
    
    signal(SIGINT, handle_sigint);
    
    printf("=== SERVIDOR WEB HTTP (em C) ===\n");
    printf("Porta: %d\n", port);
    printf("Diretorio raiz: ./www/\n");
    printf("Pool de threads: %d workers\n", THREAD_POOL_SIZE);
    printf("Fila maxima: %d conexoes\n", MAX_QUEUE_SIZE);
    printf("================================\n\n");
    
    server.logger = tslog_init("web_server.log");
    pthread_mutex_init(&server.stats_mutex, NULL);
    server.total_requests = 0;
    server.request_id = 0;
    
    // Inicializa fila de trabalho
    server.work_queue = work_queue_init();
    if (!server.work_queue) {
        fprintf(stderr, "Erro ao criar fila de trabalho\n");
        return 1;
    }
    
    tslog_info(server.logger, "=== Servidor iniciado ===");
    
    // Cria pool de threads
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Criando pool com %d threads...", THREAD_POOL_SIZE);
    tslog_info(server.logger, log_msg);
    
    int thread_ids[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        thread_ids[i] = i + 1;
        if (pthread_create(&server.thread_pool[i], NULL, worker_thread, &thread_ids[i]) != 0) {
            fprintf(stderr, "Erro ao criar thread worker %d\n", i);
            return 1;
        }
    }
    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        tslog_error(server.logger, "Erro no bind");
        return 1;
    }
    
    if (listen(server_socket, MAX_PENDING) < 0) {
        tslog_error(server.logger, "Erro no listen");
        return 1;
    }
    
    snprintf(log_msg, sizeof(log_msg), "Escutando em http://localhost:%d", port);
    tslog_info(server.logger, log_msg);
    printf("Pressione Ctrl+C para encerrar\n\n");
    
    while (g_running) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            if (errno == EINTR && !g_running) break;
            continue;
        }
        
        request_t *req = malloc(sizeof(request_t));
        if (!req) {
            close(client_socket);
            continue;
        }
        
        req->socket = client_socket;
        req->id = get_next_request_id(); // Thread-safe agora!
        
        // Adiciona à fila de trabalho
        if (work_queue_push(server.work_queue, req) < 0) {
            // Fila cheia, rejeita conexão
            const char *msg = "503 Service Unavailable\r\nConnection: close\r\n\r\nServidor sobrecarregado";
            send(client_socket, msg, strlen(msg), 0);
            close(client_socket);
            free(req);
            
            tslog_error(server.logger, "Fila de trabalho cheia - conexao rejeitada");
        }
    }
    
    tslog_info(server.logger, "=== Servidor finalizando... ===");
    
    // Sinaliza threads para parar
    pthread_cond_broadcast(&server.work_queue->not_empty);
    
    // Aguarda threads terminarem
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(server.thread_pool[i], NULL);
    }
    
    close(server_socket);
    work_queue_destroy(server.work_queue);
    pthread_mutex_destroy(&server.stats_mutex);
    tslog_destroy(server.logger);
    printf("\nServidor encerrado.\n");
    return 0;
}