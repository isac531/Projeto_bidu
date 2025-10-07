#include "libtslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

logger_t* tslog_init(const char *filename) {
    logger_t *logger = malloc(sizeof(logger_t));
    if (!logger) return NULL;
    
    // Inicializa mutex
    if (pthread_mutex_init(&logger->mutex, NULL) != 0) {
        free(logger);
        return NULL;
    }
    
    // Abre arquivo (cria se não existir no WSL)
    logger->file = fopen(filename, "a");
    if (!logger->file) {
        pthread_mutex_destroy(&logger->mutex);
        free(logger);
        return NULL;
    }
    
    return logger;
}

void tslog_destroy(logger_t *logger) {
    if (!logger) return;
    
    pthread_mutex_lock(&logger->mutex);
    if (logger->file) {
        fclose(logger->file);
    }
    pthread_mutex_unlock(&logger->mutex);
    
    pthread_mutex_destroy(&logger->mutex);
    free(logger);
}

static void write_log(logger_t *logger, const char *level, const char *message) {
    if (!logger || !message) return;
    
    pthread_mutex_lock(&logger->mutex);
    
    // Timestamp simples
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    // Escreve: [timestamp] LEVEL: message
    fprintf(logger->file, "[%s] %s: %s\n", timestamp, level, message);
    fflush(logger->file);
    
    pthread_mutex_unlock(&logger->mutex);
}

void tslog_info(logger_t *logger, const char *message) {
    write_log(logger, "INFO", message);
}

void tslog_error(logger_t *logger, const char *message) {
    write_log(logger, "ERROR", message);
}