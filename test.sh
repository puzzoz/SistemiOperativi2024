#!/bin/bash
phase=3
if [ -n "$1" ]
then
	phase=$1
fi
mkdir -p build && (cd build && make clean && cmake .. -DPHASE=$phase && make) && uriscv-cli
