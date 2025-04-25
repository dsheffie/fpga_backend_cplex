#!/bin/bash
opt -load ./schedPass.so -schedPass -useILP=true -reuseRegisters=true -numMemPipes=1 < ./"$@" > /dev/null 
