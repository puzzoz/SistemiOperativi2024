#!/bin/bash
phase=3
config=config_machine.json
if [ -n "$1" ]
then
	phase=$1
fi
if [ $phase -eq 3 ]
then
	config=config_machine_phase3.json
	echo "making devices..."
  (cd testers && make)
fi
echo "compiling tests for phase $phase..."
mkdir -p build && (cd build && make clean ; cmake .. -DPHASE=$phase && make)
echo "launching uriscv emulator with configuration file $config..."
uriscv-cli $config
