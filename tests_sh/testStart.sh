#!/bin/bash

### Testovací skript pro DNS resolver ###

binaryPath="../dns" # Cesta k binárce resolveru
timeout=1 # Timeout pro čekání na spuštění (v sekundách)

testCount=0
passedCount=0
test() {
    local name=""
    local server="1.1.1.1"
    local port="1053"
    local filters="filters.txt"
    local verbose=1
    local help=0
    local additional=""
    local error=0
    local output=""

    for arg in "$@"; do
        case $arg in
            name=*) name="${arg#*=}" ;;
            server=*) server="${arg#*=}" ;;
            port=*) port="${arg#*=}" ;;
            filters=*) filters="${arg#*=}" ;;
            verbose=*) verbose="${arg#*=}" ;;
            help=*) help="${arg#*=}" ;;
            additional=*) additional="${arg#*=}" ;;
            error=*) error="${arg#*=}" ;;
            output=*) output="${arg#*=}" ;;
        esac
    done

    ((testCount++))
    echo "=== Test $testCount: $name ==="

    # spustíme resolver a počkáme
    args=()
    [[ -n $server ]] && args+=("-s" "$server")
    [[ -n $port ]] && args+=("-p" "$port")
    [[ -n $filters ]] && args+=("-f" "$filters")
    [[ $verbose == 1 ]] && args+=("-v")
    [[ $help == 1 ]] && args+=("-h")
    [[ -n $additional ]] && args+=($additional)
    echo "Spouštím: $binaryPath ${args[@]}"
    out=$(
        { $binaryPath "${args[@]}" &
        PID=$!
        sleep $timeout
        kill -INT $PID
        wait $PID
        } 2>&1
    )

    ret=$?

    # kontrola návratového kódu
    if [[ $ret != $error ]]; then
        echo "❌ Neúspěch: očekávaný výstupní kód je: $error, ale byl: $ret."
        echo "Skutečný výstup byl:"
        echo "$out"
        echo ""
        return 1
    fi

    # kontrola očekávaného výstupu
    if [[ -n $output && $out != *"$output"* ]]; then
        echo "❌ Neúspěch: obsahovat očekávaného výstupu je: '$output'."
        echo "Skutečný výstup byl:"
        echo "$out"
        echo ""
        return 1
    fi

    echo "✅ Úspěch"
    echo ""
    ((passedCount++))
    return 0
}

### Definice testů ###

run() {
    #1
    test    name="Korektní spuštění"\
            output="Klient naslouchá na portu"
    #2
    test    name="Verbose bez -v"\
            verbose=0\
    #3
    test    name="Neplatný port serveru (mimo rozsah)"\
            port="70000"\
            error=1\
            output="Port musí být v rozsahu 1-65535"
    #4
    test    name="Neplatný port serveru (textová hodnota)"\
            port=""\
            additional="-p abc"\
            error=1\
            output="Neplatný port: abc"
    #5
    test    name="Chybějící argument hodnoty za -p"\
            port=""\
            additional="-p"\
            error=1\
            output="Neplatný či duplicitní argument"
    #6
    test    name="Chybějící hodnota posledního argumentu"\
            filters=""\
            additional="-f"\
            error=1\
            output="Neplatný či duplicitní argument"
    #7
    test    name="Neexistující soubor s filtry"\
            filters="nonexistent.txt"\
            error=1\
            output="Soubor neexistuje nebo není čitelný"
    #8
    test    name="Neplatný argument"\
            additional="-t 1000"\
            error=1\
            output="Neplatný či duplicitní argument"
    #9
    test    name="Opakovaný argument -s"\
            additional="-s 8.8.8.8"\
            error=1\
            output="Neplatný či duplicitní argument"
    #10
    test    name="Zobrazení nápovědy"\
            server=""\
            port=""\
            filters=""\
            verbose=0\
            help=1\
            output="Použití: dns -"
    #11
    test    name="Prázdné spuštění (výchozí nápověda)"\
            server=""\
            port=""\
            filters=""\
            verbose=0\
            output="Použití: dns -"
    #12
    test    name="Nápověda s ostatními argumenty"\
            help=1\
            error=1\
            output="Argument -h nemůže být kombinován s jinými argumenty"
    #13
    test    name="Chybějící argument hodnoty za -s"\
            server=""\
            verbose=0\
            additional="-s -v"\
            error=1\
            output="Chybná adresa serveru"
    #14
    test    name="Neplatná adresa serveru"\
            server="chyba"\
            error=1\
            output="Chybná adresa serveru"
    #15
    test    name="Doménové jméno serveru"\
            server="one.one.one.one"\
            output="Klient naslouchá na portu"
    #16
    test    name="IPv6 adresa serveru"\
            server="2001:4860:4860::8888"\
            output="Klient naslouchá na portu"
    #17
    test    name="Chybějící adresa serveru"\
            server=""\
            error=1\
            output="Chybí povinný argument -s server"
    #18
    test    name="Chybějící povinný argument -f"\
            filters=""\
            error=1\
            output="Chybí povinný argument -f file"
    #19
    test    name="Neplatný filtr v souboru ('-' na konci labelu)"\
            filters="invalid1.txt"\
            error=2\
            output="Neplatný filtr v souboru"
    #20
    test    name="Neplatný filtr v souboru (dvě tečky za sebou)"\
            filters="invalid2.txt"\
            error=2\
            output="Neplatný filtr v souboru"
}

### Spuštění testů ###

echo "Spouštím testy startu DNS resolveru..."
echo ""

run

### Vyhodnocení výsledků ###

if [[ $testCount == $passedCount ]]; then
    echo "Všech $passedCount testů bylo úspěšných! 🎉"
else
    echo "$passedCount z $testCount testů bylo úspěšných."
fi
echo ""

exit 0
#TODO Vlastni domeny nekdy nefunguji!!!