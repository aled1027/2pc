#!/usr/bin/env bash

set -e

mkdir -p logs

prog=./src/compgc

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f build)/lib

times=$1
if [ x"$times" == x"" ]; then
    times=10
fi

echo -e "Repeating $times times..."

for type in AES #AUD_NB # WDBC CREDIT NURSERY_DT ECG_DT WDBC_NB NURSERY_NB AUD_NB
do

    echo -e "\n$type Full\n"

    $prog --type $type --times $times --garb-full & # 2> logs/$type-garb-full.txt &
    sleep 1
    $prog --type $type --times $times --eval-full # 2> logs/$type-eval-full.txt

    echo -e "\n$type Offline/Online\n"

    rm -f function/garbler_gcs/*
    rm -f function/evaluator_gcs/*

    ./src/compgc --type $type --garb-off & # 2> logs/$type-garb-off.txt 1>/dev/null &
    sleep 1
    ./src/compgc --type $type --eval-off # 2> logs/$type-eval-off.txt 1>/dev/null

    echo -e "\n$type Online\n"

    ./src/compgc --type $type --times $times --garb-on & # 2> logs/$type-garb-on.txt &
    sleep 1
    ./src/compgc --type $type --times $times --eval-on # 2> logs/$type-eval-on.txt
done

