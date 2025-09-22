#!/bin/bash

# Testovací skript pre DNS server s vláknami
# Spustí 100 simultánnych DNS dotazov

SERVER_IP="127.0.0.1"
PORT="1234"
DOMAIN="google.com"
QUERY_TYPE="A"

echo "Testing DNS server with threads..."
echo "Server: $SERVER_IP:$PORT"
echo "Domain: $DOMAIN"
echo ""

# Funkcia pre jednotlivý dotaz
run_dig() {
    local query_num=$1
    echo "Query $query_num started at: $(date +%H:%M:%S.%N)"
    dig @$SERVER_IP -p $PORT $DOMAIN $QUERY_TYPE +time=1 +tries=1 > /tmp/dns_query_$query_num.txt 2>&1
    local exit_code=$?
    echo "Query $query_num finished at: $(date +%H:%M:%S.%N) (exit code: $exit_code)"
    return $exit_code
}

# Spustíme 100 dotazov na pozadí
echo "Starting 100 simultaneous DNS queries..."
for i in {1..10}; do
    run_dig $i &
    # Malý delay medzi spusteniami pre lepšiu simultánnosť
    sleep 0.01
done

# Počkáme na dokončenie všetkých dotazov
echo ""
echo "Waiting for all queries to complete..."
wait

# Zhrnutie výsledkov
echo ""
echo "=== RESULTS SUMMARY ==="
success_count=0
fail_count=0

for i in {1..10}; do
    if grep -q "ANSWER SECTION" /tmp/dns_query_$i.txt; then
        echo "Query $i: SUCCESS"
        ((success_count++))
    else
        echo "Query $i: FAILED"
        ((fail_count++))
        # Vypíšeme chybu pre failed queries
        echo "  Error: $(grep -i "failed\|timeout\|error" /tmp/dns_query_$i.txt | head -1)"
    fi
done

echo ""
echo "Total: $success_count successful, $fail_count failed"

# Vyčistenie
rm -f /tmp/dns_query_*.txt

echo "Test completed."