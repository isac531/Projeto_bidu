#ifndef LIBTSLOG_H
#define LIBTSLOG_H

#include <stdio.h>
#include <pthread.h>

// Níveis de log simples
typedef enum {
    LOG_INFO = 0,
    LOG_ERROR = 1
} log_level_t;

// Estrutura do logger simples
typedef struct {
    FILE *file;
    pthread_mutex_t mutex;
} logger_t;

// Funções principais
logger_t* tslog_init(const char *filename);
void tslog_destroy(logger_t *logger);
void tslog_info(logger_t *logger, const char *message);
void tslog_error(logger_t *logger, const char *message);

#endif // LIBTSLOG_H