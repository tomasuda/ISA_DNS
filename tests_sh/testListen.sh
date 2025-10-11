#!/bin/bash

binaryPath="../dns" # Cesta k binárce resolveru
timeout=1 # Timeout pro čekání na spuštění obsloužení (v sekundách)
IPv6=1 # 1 = testy s IPv6, 0 = testy bez IPv6

testCount=0
passedCount=0
test() {
    local name=""
    local domain="example.com"
    local port="1053"
    local filters="filters.txt"
    local type="A"
    local rcode="NOERROR"
    local outputDig=""
    local repeats=1

    for arg in "$@"; do
        case $arg in
            name=*) name="${arg#*=}" ;;
            domain=*) domain="${arg#*=}" ;;
            port=*) port="${arg#*=}" ;;
            filters=*) filters="${arg#*=}" ;;
            type=*) type="${arg#*=}" ;;
            rcode=*) rcode="${arg#*=}" ;;
            outputDig=*) outputDig="${arg#*=}" ;;
            repeats=*) repeats="${arg#*=}" ;;
        esac
    done

    single() {
        echo Spouštím: dig @"$host" -p $port $domain $type +tries=1 +notcp
        outDig=$(dig @"$host" -p "$port" "$domain" "$type" +tries=1 +notcp 2>&1)
        retDig=$?

        sleep $timeout

        if [[ $retDig != 0 ]]; then
            echo "❌ Neúspěch: očekávaný výstupní kód dig je: 0, ale byl: $retDig."
            echo "Výstup dig byl:"
            echo "$outDig"
            echo ""
            return 1
        fi

        if [[ -n $rcode && $outDig != *"status: $rcode"* ]]; then
            echo "❌ Neúspěch: očekávaný RCODE je: '$rcode'."
            echo "Skutečný výstup dig byl:"
            echo "$outDig"
            echo ""
            return 1
        fi

        if [[ -n $outputDig && $outDig != *"$outputDig"* ]]; then
            echo "❌ Neúspěch: očekávaný výstup je: '$outputDig'."
            echo "Skutečný výstup dig byl:"
            echo "$outDig"
            echo ""
            return 1
        fi

        return 0
    }

    multiple() {
        echo "Spouštím $repeats krát: dig @"$host" -p $port $domain $type +tries=1 +notcp"

        pidsDig=()
        for i in $(seq 1 $repeats); do
            dig @"$host" -p "$port" "$domain" "$type" +tries=1 +notcp | grep "status: $rcode" >/dev/null 2>&1 &
            pidsDig+=($!)
        done

        sleep $timeout

        retsDig=()
        for p in "${pidsDig[@]}"; do
            wait "$p"
            retsDig+=($?)
        done

        failed=0
        for r in "${retsDig[@]}"; do
            if (( r != 0 )); then ((failed++)); fi
        done

        if [[ $failed != 0 ]]; then
            echo "❌ Neúspěch: $failed z $repeats dotazů selhalo."
            echo ""
            return 1
        fi

        return 0
    }

    ((testCount++))
    echo "=== Test $testCount ($part): $name ==="

    echo "Spouštím: $binaryPath -s $server -p $port -f $filters"
    "$binaryPath" -s "$server" -p "$port" -f "$filters" >/dev/null &
    pid=$!
    sleep $timeout

    if [[ $repeats == 1 ]]; then
        single
        res=$?
    else
        multiple
        res=$?
    fi

    kill -INT $pid
    wait $pid
    ret=$?

    if [[ $res != 0 ]]; then
        return 1
    fi

    if [[ $ret != 0 ]]; then
        echo "❌ Neúspěch: očekávaný výstupní kód je: 0, ale byl: $ret."
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
    #1/14
    test    name="Korektní doména"\
            outputDig="23.192.228.80"
    #2/15
    test    name="Filtrovaná doména"\
            domain="zmedia.com"\
            rcode="REFUSED"
    #3/16
    test    name="Filtrovaná poddoména"\
            domain="sub.my.public.wikia-ads.wikia.com"\
            rcode="REFUSED"
    #4/17
    test    name="Neexistující doména"\
            domain="neexistujici1234567890.cz"\
            rcode="NXDOMAIN"
    #5/18
    test    name="Korektní dotaz typu IPv6"\
            type="AAAA"\
            outputDig="2600:1408:ec00:36::1736:7f24"
    #6/19
    test    name="Nevalidní doména (končí pomlčkou)"\
            domain="domain-.com"\
            rcode="NXDOMAIN"
    #7/20
    test    name="Nevalidní doména (neznámé znaky)"\
            domain="my_address.io"\
            rcode="NXDOMAIN"
    #8/21
    test    name="Root doména"\
            domain="."
    #9/22
    test    name="Blokování domény v odlišném zápisu od filtru"\
            domain="URL9467.COMMS-2.ZOoPLa.CO.UK."\
            rcode="REFUSED"\
            outputDig=""
    #10/23
    test    name="Blokování všech domén"\
            filters="filtersAll.txt"\
            rcode="REFUSED"
    #11/24
    test    name="Zátěž s více dotazy"\
            repeats=100
    #12/25
    test    name="Zátěž s více filtrovanými dotazy"\
            domain="mybbc-analytics.files.bbci.co.uk"\
            rcode="REFUSED"\
            repeats=100
    #13/26
    test    name="Zátěž s neplatnými doménami"\
            domain="invalíd_domain.cz"\
            rcode="NXDOMAIN"\
            repeats=100
}

### Spuštění testů ###

echo "Spouštím testy poslouchání DNS resolveru..."
echo ""

part="IPv4"
server="8.8.8.8"
host="127.0.0.1"
run

if [[ $IPv6 == 1 ]]; then
    part="IPv6"
    server="2001:4860:4860::8888"
    host="::1"
    run
fi

### Vyhodnocení výsledků ###

if [[ $testCount == $passedCount ]]; then
    echo "Všech $passedCount testů bylo úspěšných! 🎉"
else
    echo "$passedCount z $testCount testů bylo úspěšných."
fi
echo ""

exit 0