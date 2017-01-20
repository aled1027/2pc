#!/usr/bin/env bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f build)/lib

valgrind --leak-check=full ./src/compgc "$@"
