CC = gcc
CFLAGS = -Wall -pthread -g
LDFLAGS = -pthread

# Objetos
LOGGER_OBJ = libtslog.o
SERVER_OBJ = web_server.o
CLIENT_OBJ = web_client.o

# Executáveis
SERVER = web_server
CLIENT = web_client

all: $(SERVER) $(CLIENT)

# Logger library
libtslog.o: libtslog.c libtslog.h
	$(CC) $(CFLAGS) -c libtslog.c -o libtslog.o

# Servidor
$(SERVER): $(SERVER_OBJ) $(LOGGER_OBJ)
	$(CC) $(SERVER_OBJ) $(LOGGER_OBJ) -o $(SERVER) $(LDFLAGS)

web_server.o: web_server.c libtslog.h
	$(CC) $(CFLAGS) -c web_server.c -o web_server.o

# Cliente
$(CLIENT): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $(CLIENT)

web_client.o: web_client.c
	$(CC) $(CFLAGS) -c web_client.c -o web_client.o

# Testes
test: all
	@echo "=== Como Testar ==="
	@echo ""
	@echo "1. Em um terminal, inicie o servidor:"
	@echo "   ./$(SERVER)"
	@echo ""
	@echo "2. Em outro terminal, teste com o cliente:"
	@echo "   ./$(CLIENT) /"
	@echo "   ./$(CLIENT) /stats"
	@echo "   ./$(CLIENT) /about"
	@echo ""
	@echo "3. Ou use curl:"
	@echo "   curl http://localhost:8080/"
	@echo "   curl http://localhost:8080/stats"
	@echo ""
	@echo "4. Teste automatizado com multiplos clientes:"
	@echo "   chmod +x test_web.sh"
	@echo "   ./test_web.sh"
	@echo ""
	@echo "5. Ver logs em tempo real:"
	@echo "   tail -f web_server.log"

clean:
	rm -f $(SERVER) $(CLIENT) *.o *.log

.PHONY: all test clean