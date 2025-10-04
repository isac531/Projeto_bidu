#!/bin/bash

# Script de teste para simular múltiplos clientes

HOST="127.0.0.1"
PORT=5555
NUM_CLIENTS=5

echo "===================================="
echo "  Teste de Multiplos Clientes Web"
echo "===================================="
echo "Host: $HOST:$PORT"
echo "Clientes simultaneos: $NUM_CLIENTS"
echo "===================================="
echo ""

# Verifica se curl está instalado
if ! command -v curl &> /dev/null; then
    echo "ERRO: curl nao encontrado."
    echo "Instale com: sudo apt-get install curl"
    exit 1
fi

# Verifica se servidor está rodando
echo "Verificando servidor..."
if ! curl -s -o /dev/null --connect-timeout 2 "http://$HOST:$PORT/" 2>/dev/null; then
    echo "ERRO: Servidor nao esta rodando em http://$HOST:$PORT"
    echo ""
    echo "Inicie o servidor primeiro:"
    echo "  ./web_server"
    exit 1
fi

echo "Servidor OK!"
echo ""

# Função para simular um cliente
test_client() {
    local id=$1
    echo "[Cliente $id] Iniciando..."
    
    # Testa diferentes rotas
    for path in "/" "/stats" "/about" "/naoexiste"; do
        echo "[Cliente $id] GET $path"
        response=$(curl -s -o /dev/null -w "%{http_code}" "http://$HOST:$PORT$path" 2>/dev/null)
        echo "[Cliente $id] Resposta: HTTP $response"
        sleep 0.1
    done
    
    echo "[Cliente $id] Finalizado"
}

# Executa múltiplos clientes em paralelo
echo "Iniciando $NUM_CLIENTS clientes em paralelo..."
echo ""

for i in $(seq 1 $NUM_CLIENTS); do
    test_client $i &
done

# Aguarda todos terminarem
wait

echo ""
echo "===================================="
echo "  Teste Concluido!"
echo "===================================="
echo ""

# Mostra estatísticas do servidor
echo "Estatisticas do servidor:"
curl -s "http://$HOST:$PORT/stats"
echo ""
echo ""
echo "Verifique o arquivo web_server.log para logs detalhados"
echo "Comando: tail -f web_server.log"