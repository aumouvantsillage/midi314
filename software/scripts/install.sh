#!/bin/bash

DIR=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)

PREFIX=${PREFIX:-/usr}
BIN=$PREFIX/bin
SHARE=$PREFIX/share/midi314

mkdir -p $BIN
cp $DIR/../midi314-display/target/release/midi314-display $BIN
cp $DIR/../midi314-looper/target/release/midi314-looper   $BIN
cp $DIR/../scripts/midi314.sh                             $BIN

mkdir -p $SHARE
cp $DIR/midi314.qjackctl $SHARE
cp $DIR/midi314.rules    $SHARE
