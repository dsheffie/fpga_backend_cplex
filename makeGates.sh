#!/bin/bash
opt -load ./schedPass.so -schedPass -useILP=false -reuseRegisters=false -numMemPipes=1 < ./"$@" > /dev/null 
