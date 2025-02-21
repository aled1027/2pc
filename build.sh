#!/usr/bin/env bash

set -e

mkdir -p build/autoconf
builddir=$(readlink -f build)
if [ "$1" == "debug" ]; then
    debug="--enable-debug"
else
    debug=""
fi

mkdir -p function/garbler_gcs
mkdir -p function/evaluator_gcs

export CPPFLAGS=-I$builddir/include
export CFLAGS=-I$builddir/include
export LDFLAGS=-L$builddir/lib

build () {
    echo building $1
    path=$1
    url=$2
    branch=$3
    if [ ! -d $path ]; then
        git clone $url $path;
    fi
    pushd $path
        git pull origin $branch
        mkdir -p build/autoconf
        autoreconf -i
        ./configure --prefix=$builddir $debug
        make
        make install
    popd
}

echo
echo builddir = $builddir
echo

build libgarble    https://github.com/amaloz/libgarble master

autoreconf -i
./configure --prefix=$builddir $debug
make
