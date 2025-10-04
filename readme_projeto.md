# Mini Servidor Web HTTP em C

## Etapa 1 — Biblioteca de Logging (libtslog)

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

## Etapa 2 — Protótipo Servidor Web HTTP (v2-web)

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
* **Threads (pthread)**: Uma thread por conexão cliente.
* **Exclusão mútua (mutex)**: Proteção de variáveis compartilhadas (estatísticas).
* **Sockets TCP/IP**: Servidor escuta na porta configurável (padrão: 8080).
* **Logging thread-safe**: Todas operações registradas via libtslog.
* **Gerenciamento de recursos**: Fechamento de sockets e liberação de memória.
* **Tratamento de erros**: Verificação de retorno de syscalls com mensagens claras.

### Rotas Disponíveis
| Rota | Método | Descrição |
|------|--------|-----------|
| `/` | GET | Página inicial com estatísticas do servidor |
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
./web_client /about 127.0.0.1 8080
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
[02:42:09] INFO: Escutando na porta 8080
[02:42:09] INFO: [REQ #1] GET /
[02:42:09] INFO: [RES #1] 200 OK - /
[02:42:09] INFO: [REQ #2] GET /stats
[02:42:09] INFO: [RES #2] 200 OK - /stats
[02:42:09] INFO: [REQ #3] GET /about
[02:42:09] INFO: [RES #3] 200 OK - /about
[02:42:09] INFO: [REQ #4] GET /naoexiste
[02:42:09] INFO: [RES #4] 404 - /naoexiste
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
└── README.md               # Esta documentação
```

---

## Status

* Etapa 1 concluída (biblioteca libtslog + teste multi-thread)
* Etapa 2 concluída (servidor web HTTP + cliente + logging + scripts)