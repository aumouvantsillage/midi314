#!/bin/sh

PREFIX=${PREFIX:-/usr}
BIN=$PREFIX/bin
SHARE=$PREFIX/share/midi314

cp software/midi314-display/target/release/midi314-display $BIN
cp software/midi314-looper/target/release/midi314-looper $BIN
cp software/scripts/midi314.sh $BIN

mkdir -p $SHARE
cp software/scripts/midi314.qjackctl $SHARE
cp software/scripts/midi314.rules $SHARE
