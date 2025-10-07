
## Etapa 1 – Biblioteca de Logging (libtslog)

### Objetivos
* Implementar biblioteca de logging **thread-safe** com API simples em C.
* Teste CLI simulando múltiplas threads gravando logs concorrentes.

### Implementação
* Arquivos principais:
   * `libtslog.h` / `libtslog.c`: biblioteca de logging baseada em `pthread_mutex_t`.
   * `test_logger.c`: programa de teste que cria múltiplas threads e grava mensagens de log concorrentes.
   * `makefile`: sistema de build para biblioteca e testes.

### Funcionalidades
* Níveis de log: **INFO, ERROR**.
* Saída para arquivo configurável (ex: `test.log`).
* Garantia de segurança em concorrência via `pthread_mutex_t`.
* Timestamp automático em formato `HH:MM:SS`.

### Teste

```bash
make
./test_logger [arquivo.log] [num_threads]
```

Exemplo:
```bash
./test_logger basic.log 3
```

Saída esperada: mensagens intercaladas de várias threads no arquivo de log com timestamps.

---

## Etapa 2 – Protótipo Servidor Web HTTP (v2-web)

### Objetivos
* Implementar servidor web HTTP mínimo respondendo a requisições GET.
* Integrar logging da Etapa 1.
* Fornecer scripts de teste para múltiplos clientes simultâneos.

### Implementação
* Arquivos principais:
   * `web_server.c`: servidor HTTP TCP concorrente. Aceita múltiplas conexões (thread por conexão), processa requisições GET e registra logs.
   * `web_client.c`: cliente HTTP que faz requisições GET ao servidor.
   * `libtslog.h` / `libtslog.c`: logging thread-safe integrado.
   * Scripts:
      * `Makefile`: compila servidor e cliente.
      * `test_web.sh`: executa teste com múltiplos clientes simultâneos.

### Recursos Implementados
* **Pool de Threads**: Pool fixo de 10 threads workers (configurável) para limitar recursos.
* **Fila de Trabalho Thread-Safe**: Fila circular com limite de 100 conexões pendentes.
* **Exclusão mútua (mutex)**: Proteção de variáveis compartilhadas (estatísticas, request_id).
* **Sockets TCP/IP**: Servidor escuta na porta configurável (padrão: 8080).
* **Logging thread-safe**: Todas operações registradas via libtslog.
* **Gerenciamento de recursos**: Fechamento de sockets e liberação de memória.
* **Tratamento de erros**: Verificação de retorno de syscalls com mensagens claras.
* **Proteção contra sobrecarga**: Retorna 503 quando fila está cheia.

### Rotas Disponíveis
| Rota | Método | Descrição |
|------|--------|-----------|
| `/` | GET | Página inicial (index.html) |
| `/stats` | GET | Estatísticas em formato JSON |
| `/about` | GET | Informações sobre recursos implementados |

### Teste Rápido

1. **Compile:**
```bash
make
```

2. **Inicie o servidor:**
```bash
./web_server [porta]
```
Exemplo:
```bash
./web_server 8080
```

3. **Teste com cliente:**
```bash
./web_client / 127.0.0.1 8080
./web_client /stats 127.0.0.1 8080
./web_client /about.html 127.0.0.1 8080
```

4. **Ou use curl:**
```bash
curl http://localhost:8080/
curl http://localhost:8080/stats
curl http://localhost:8080/about
```

5. **Teste automatizado com múltiplos clientes:**
```bash
chmod +x test_web.sh
./test_web.sh
```

O script simula 5 clientes simultâneos, cada um fazendo 4 requisições (total: 20 requisições).

### Logs
* Todas as conexões e requisições são registradas em `web_server.log`.
* Ver logs em tempo real:
```bash
tail -f web_server.log
```

### Exemplo de Log
```
[02:42:09] INFO: === Servidor iniciado ===
[02:42:09] INFO: Escutando em http://localhost:8080
[02:42:09] INFO: [REQ #1] GET /
[02:42:09] INFO: [RES #1] 200 OK - www/index.html
[02:42:09] INFO: [REQ #2] GET /stats
[02:42:09] INFO: [RES #2] 200 OK - www/stats
[02:42:09] INFO: [REQ #3] GET /about
[02:42:09] INFO: [RES #3] 200 OK - www/about
[02:42:09] INFO: [REQ #4] GET /naoexiste
[02:42:09] INFO: [RES #4] 404 - www/naoexiste
```

### Limpeza
```bash
make clean
```

Remove executáveis, objetos e logs.

---

## Estrutura de Arquivos

```
projeto/
├── libtslog.h              # Header da biblioteca de log
├── libtslog.c              # Implementação do logger thread-safe
├── test_logger.c           # Teste da biblioteca de log
├── web_server.c            # Servidor HTTP (Etapa 2)
├── web_client.c            # Cliente HTTP (Etapa 2)
├── test_web.sh             # Script de teste automatizado
├── Makefile                # Sistema de build
├── www/                    # Diretório de conteúdo web
│   ├── index.html          # Página inicial
│   ├── about.html          # Página sobre
│   └── ...                 # Outros arquivos estáticos
└── README.md               # Esta documentação
```

---

## Relatório Final

### 1. Diagrama de Sequência Cliente-Servidor

```
Cliente                         Servidor                      Logger
   |                               |                             |
   |---(1) socket()--------------->|                             |
   |                               |                             |
   |---(2) connect()-------------->|                             |
   |                               |---(3) accept()              |
   |                               |                             |
   |                               |---(4) pthread_create()      |
   |                               |        (nova thread)        |
   |                               |                             |
   |---(5) send(GET /path)-------->|                             |
   |                               |                             |
   |                               |---(6) recv()                |
   |                               |                             |
   |                               |---(7) parse request         |
   |                               |     (method, path)          |
   |                               |                             |
   |                               |---(8) tslog_info()--------->|
   |                               |     "[REQ] GET /path"       |
   |                               |                             |
   |                               |                             |---(9) pthread_mutex_lock()
   |                               |                             |---(10) fprintf(timestamp + msg)
   |                               |                             |---(11) fflush()
   |                               |                             |---(12) pthread_mutex_unlock()
   |                               |<----------------------------|
   |                               |                             |
   |                               |---(13) stat(file_path)      |
   |                               |                             |
   |                               |---(14) read_file()          |
   |                               |                             |
   |                               |---(15) get_mime_type()      |
   |                               |                             |
   |                               |---(16) send_http_response() |
   |<--(17) send(HTTP headers)-----|                             |
   |<--(18) send(content)----------|                             |
   |                               |                             |
   |                               |---(19) tslog_info()-------->|
   |                               |     "[RES] 200 OK"          |
   |                               |                             |
   |                               |                             |---(20) log escrito
   |                               |<----------------------------|
   |                               |                             |
   |                               |---(21) close(socket)        |
   |                               |---(22) free(request)        |
   |                               |---(23) pthread_exit()       |
   |                               |                             |
   |---(24) recv() [fim]---------->|                             |
   |                               |                             |
   |---(25) close()--------------->|                             |
   |                               |                             |
```

**Fluxo Detalhado:**

1. **Cliente cria socket** TCP
2. **Cliente conecta** ao servidor (IP:porta)
3. **Servidor aceita** conexão com `accept()`
4. **Servidor cria nova thread** para tratar o cliente
5. **Cliente envia requisição** HTTP GET
6. **Servidor recebe** dados com `recv()`
7. **Servidor parseia** método, caminho e versão HTTP
8. **Servidor registra log** da requisição (thread-safe)
9-12. **Logger protege escrita** com mutex e grava no arquivo
13. **Servidor verifica** se arquivo existe com `stat()`
14. **Servidor lê arquivo** do disco
15. **Servidor determina** tipo MIME baseado na extensão
16-18. **Servidor envia resposta** HTTP (headers + conteúdo)
19-20. **Servidor registra log** da resposta
21-23. **Servidor limpa recursos** da thread
24-25. **Cliente fecha conexão**

---

### 2. Mapeamento Requisitos → Código

#### 2.1 Requisitos Funcionais

| Requisito | Descrição | Implementação | Arquivo | Linhas |
|-----------|-----------|---------------|---------|--------|
| **RF1** | Servidor HTTP aceita conexões TCP | `socket()`, `bind()`, `listen()`, `accept()` | `web_server.c` | 167-191 |
| **RF2** | Processar requisições GET | Parsing do request, validação do método | `web_server.c` | 87-106 |
| **RF3** | Servir arquivos estáticos | `read_file()`, `stat()` | `web_server.c` | 52-68, 115-141 |
| **RF4** | Retornar códigos HTTP corretos | 200, 400, 404, 405, 500 | `web_server.c` | 70-86, 108-141 |
| **RF5** | Suportar tipos MIME | `get_mime_type()` com extensões comuns | `web_server.c` | 33-47 |
| **RF6** | Logging thread-safe de operações | `tslog_init()`, `tslog_info()`, `tslog_error()` | `libtslog.c` | 5-71 |
| **RF7** | Múltiplos clientes simultâneos | Pool de threads + fila thread-safe | `web_server.c` | 59-142, 280-334 |

#### 2.2 Requisitos Não-Funcionais

| Requisito | Descrição | Implementação | Arquivo | Linhas |
|-----------|-----------|---------------|---------|--------|
| **RNF1** | Thread-safety no logging | `pthread_mutex_t` protege escrita no arquivo | `libtslog.c` | 36-52 |
| **RNF2** | Tratamento de erros robusto | Verificação de retorno de syscalls | `web_server.c` | 169-185 |
| **RNF3** | Gerenciamento de memória | `malloc()` + `free()` para buffers dinâmicos | `web_server.c` | 57-67, 143-145 |
| **RNF4** | Encerramento gracioso | Handler `SIGINT` para shutdown limpo | `web_server.c` | 27-29, 154, 194-201 |
| **RNF5** | Porta configurável | Argumento de linha de comando | `web_server.c` | 150-151 |
| **RNF6** | Proteção de dados compartilhados | Mutex para `total_requests` e `request_id` | `web_server.c` | 210-215, 279-283 |
| **RNF7** | Timestamps em logs | `strftime()` com formato HH:MM:SS | `libtslog.c` | 42-45 |
| **RNF8** | Limite de recursos (threads) | Pool fixo de 10 threads workers | `web_server.c` | 13, 59-142, 311-320 |
| **RNF9** | Limite de fila de conexões | Fila circular com máximo de 100 requisições | `web_server.c` | 14, 59-142 |
| **RNF10** | Proteção contra sobrecarga | Retorna HTTP 503 quando fila cheia | `web_server.c` | 369-377 |

#### 2.3 Biblioteca de Logging (libtslog)

| Funcionalidade | Implementação | Arquivo | Linhas |
|----------------|---------------|---------|--------|
| Inicialização do logger | `tslog_init()`: cria estrutura, mutex e abre arquivo | `libtslog.c` | 5-23 |
| Destruição do logger | `tslog_destroy()`: fecha arquivo, destrói mutex, libera memória | `libtslog.c` | 25-36 |
| Escrita thread-safe | `write_log()`: lock → timestamp → fprintf → unlock | `libtslog.c` | 38-52 |
| Níveis de log | `tslog_info()`, `tslog_error()` | `libtslog.c` | 54-71 |
| Estruturas de dados | `logger_t` com FILE* e pthread_mutex_t | `libtslog.h` | 13-16 |

#### 2.4 Cliente HTTP

| Funcionalidade | Implementação | Arquivo | Linhas |
|----------------|---------------|---------|--------|
| Criação de socket | `socket(AF_INET, SOCK_STREAM, 0)` | `web_client.c` | 15-19 |
| Conexão ao servidor | `connect()` com `sockaddr_in` | `web_client.c` | 29-35 |
| Montagem de requisição HTTP | `snprintf()` com headers GET | `web_client.c` | 39-45 |
| Envio de dados | `send()` | `web_client.c` | 48-52 |
| Recepção de resposta | Loop `recv()` até EOF | `web_client.c` | 57-62 |

---

### 3. Relatório de Análise com IA


**Implementação da Biblioteca de Logging (libtslog.c)**
- Sugeriu uso de `pthread_mutex_t` para garantir thread-safety
- Ajudou no design da estrutura `logger_t` com mutex integrado
- Orientou implementação de timestamps com `strftime()`

**Servidor HTTP Básico (web_server.c - versão inicial)**
- Auxiliou no parsing de requisições HTTP com `sscanf()`
- Sugeriu mapeamento de extensões para tipos MIME
- Orientou gerenciamento de memória com `malloc()` e `free()`

**Identificação de Problemas Críticos**

1. **Race Condition em `request_id`** - Linha 191 (versão original)
   - IA identificou: incremento sem proteção mutex
   - Solução implementada: função `get_next_request_id()` thread-safe (linhas 279-283)

2. **Ausência de Pool de Threads**
   - IA identificou: criação ilimitada de threads
   - Solução implementada: pool com 10 workers + fila de 100 requisições (linhas 59-142)

**Implementação do Pool de Threads**
- Projetou fila circular thread-safe com condition variables
- Implementou proteção contra sobrecarga (resposta HTTP 503)
- Criou shutdown gracioso das threads

**Documentação**
- Gerou diagrama de sequência cliente-servidor
- Criou tabelas de mapeamento requisitos → código
- Elaborou análise de segurança e performance

---