#!/bin/bash

#@file:   scripts/post_power.sh 
#@brief:  Post-processing output of powerwrapper
#@author: Ippokratis Pandis

# args: <file>
if [ $# -lt 1 ]; then
    echo "Usage: $0 <file>" >&2
    exit 1
fi

EXPFILE=$1; shift

### TPCC/TPCB have TPS, while TM1 has MQTh
echo "++ Throughput"
cat $EXPFILE | ggrep -e "TPS" -e "MQTh" -e "measure" | grep -v "measurement" | grep -v "^measure" | grep -v "echo" | sed 's/^.*(//' | sed -e 's/.$//'

echo "++ AvgCPU"
cat $EXPFILE | ggrep -e "AvgCPU" -e "measure" | grep -v "measurement" | grep -v "^measure" | grep -v "echo" | sed 's/^.*(//' | sed -e 's/..$//'

echo "++ CpuLoad"
cat $EXPFILE | ggrep -e "CpuLoad" -e "measure" | grep -v "measurement" | grep -v "^measure" | grep -v "echo" | sed 's/^.*(//' | sed -e 's/.$//'

### TM1 has also SuccessRate
echo "++ SuccessRate"
cat $EXPFILE | grep "Success" | uniq | sed 's/^.*(//' | sed -e 's/..$//'


exit

