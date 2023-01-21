#!/bin/bash
ssh $1 "mkdir -p libs"
scp libs/libcrypto.so $1:libs/libcrypto.so
scp libs/libidn.so $1:libs/
scp libs/liblber.so $1:libs/
scp libs/libldap_r.so $1:libs/
scp libs/librestclient-cpp.la $1:libs/
scp libs/librestclient-cpp.so.1.1.1 $1:libs/
scp libs/libssl.so $1:libs/
scp -p mkliblinks.sh $1:libs
