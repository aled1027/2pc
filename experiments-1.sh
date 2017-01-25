#!/usr/bin/env bash

set -e

mkdir -p logs

prog=./src/compgc

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f build)/lib

times=$1
if [ x"$times" == x"" ]; then
    times=100
fi

echo -e "Repeating $times times..."

for type in AES CBC
do

    echo -e "\n$type Full\n"

    $prog --type $type --times $times --garb-full 2> logs/$type-garb-full.txt &
    sleep 1
    $prog --type $type --times $times --eval-full 2> logs/$type-eval-full.txt

    echo -e "\n$type Offline/Online\n"

    rm -f function/garbler_gcs/*
    rm -f function/evaluator_gcs/*

    ./src/compgc --type $type --garb-off 2> logs/$type-garb-off.txt 1>/dev/null &
    sleep 1
    ./src/compgc --type $type --eval-off 2> logs/$type-eval-off.txt 1>/dev/null

    echo -e "\n$type Online\n"

    ./src/compgc --type $type --times $times --garb-on 2> logs/$type-garb-on.txt &
    sleep 1
    ./src/compgc --type $type --times $times --eval-on 2> logs/$type-eval-on.txt
done

for nsymbols in 30 60; do

    echo -e "\nLEVEN Full\n"

    $prog --type LEVEN --times $times --garb-full --nsymbols $nsymbols 2> logs/LEVEN-garb-full.txt &
    sleep 1
    $prog --type LEVEN --times $times --eval-full --nsymbols $nsymbols 2> logs/LEVEN-eval-full.txt

    echo -e "\nLEVEN Offline/Online\n"

    rm -f function/garbler_gcs/*
    rm -f function/evaluator_gcs/*

    ./src/compgc --type LEVEN --garb-off --nsymbols $nsymbols 2> logs/LEVEN-garb-off.txt 1>/dev/null &
    sleep 1
    ./src/compgc --type LEVEN --eval-off --nsymbols $nsymbols 2> logs/LEVEN-eval-off.txt 1>/dev/null

    echo -e "\nLEVEN Online\n"

    ./src/compgc --type LEVEN --times $times --garb-on --nsymbols $nsymbols 2> logs/LEVEN-garb-on.txt &
    sleep 1
    ./src/compgc --type LEVEN --times $times --eval-on --nsymbols $nsymbols 2> logs/LEVEN-eval-on.txt
done
