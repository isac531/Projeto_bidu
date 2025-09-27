# Makefile simples para o logger
CC=gcc
CFLAGS=-Wall -std=c99 -pthread
LDFLAGS=-pthread

# Arquivos
LIB_SRC=libtslog.c
LIB_OBJ=libtslog.o
LIB=libtslog.a

TEST_SRC=test_logger.c
TEST_BIN=test_logger

# Regra padrão
all: $(LIB) $(TEST_BIN)

# Compila biblioteca
$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(LIB_OBJ): $(LIB_SRC) libtslog.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compila teste
$(TEST_BIN): $(TEST_SRC) $(LIB)
	$(CC) $(CFLAGS) $< -L. -ltslog $(LDFLAGS) -o $@

# Executa teste
test: $(TEST_BIN)
	@echo "=== Teste básico ==="
	./$(TEST_BIN) basic.log 2
	@echo ""
	@echo "=== Teste com mais threads ==="
	./$(TEST_BIN) multi.log 4

# Mostra logs
show-logs:
	@echo "=== Logs disponíveis ==="
	@ls -la *.log 2>/dev/null || echo "Nenhum log encontrado"
	@echo ""
	@if [ -f basic.log ]; then echo "=== basic.log ==="; cat basic.log; echo ""; fi

# Limpa tudo
clean:
	rm -f *.o *.a $(TEST_BIN) *.log

# Help
help:
	@echo "Targets disponíveis:"
	@echo "  all       - Compila tudo"
	@echo "  test      - Executa testes"
	@echo "  show-logs - Mostra logs gerados"
	@echo "  clean     - Remove arquivos gerados"
	@echo ""
	@echo "Uso manual:"
	@echo "  ./test_logger [arquivo.log] [num_threads]"

.PHONY: all test show-logs clean help