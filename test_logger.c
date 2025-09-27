#include "libtslog.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_THREADS 5

// Dados para cada thread
typedef struct {
    logger_t *logger;
    int thread_id;
} thread_data_t;

// Função da thread
void* worker_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    
    // Cada thread escreve alguns logs
    for (int i = 0; i < 10; i++) {
        char message[128];
        
        if (i % 3 == 0) {
            snprintf(message, sizeof(message), "Thread %d processou item %d", 
                    data->thread_id, i);
            tslog_info(data->logger, message);
        } else {
            snprintf(message, sizeof(message), "Thread %d erro no item %d", 
                    data->thread_id, i);
            tslog_error(data->logger, message);
        }
        
        // Pausa pequena para simular trabalho
        usleep(100000); // 100ms
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    char *log_file = "test.log";
    int num_threads = 3;
    
    // Parse simples dos argumentos
    if (argc >= 2) {
        log_file = argv[1];
    }
    if (argc >= 3) {
        num_threads = atoi(argv[2]);
        if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
    }
    
    printf("=== Teste Logger Simples ===\n");
    printf("Arquivo: %s\n", log_file);
    printf("Threads: %d\n", num_threads);
    printf("============================\n\n");
    
    // Cria logger
    logger_t *logger = tslog_init(log_file);
    if (!logger) {
        printf("Erro ao criar logger!\n");
        return 1;
    }
    
    tslog_info(logger, "=== Teste iniciado ===");
    
    // Cria threads
    pthread_t threads[MAX_THREADS];
    thread_data_t thread_data[MAX_THREADS];
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].logger = logger;
        thread_data[i].thread_id = i + 1;
        
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]) != 0) {
            printf("Erro ao criar thread %d\n", i);
            continue;
        }
        printf("Thread %d criada\n", i + 1);
    }
    
    // Aguarda threads terminarem
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d terminou\n", i + 1);
    }
    
    tslog_info(logger, "=== Teste finalizado ===");
    
    // Finaliza
    tslog_destroy(logger);
    
    printf("\nTeste concluído! Verifique o arquivo %s\n", log_file);
    
    return 0;
}