#!/bin/bash

# Print the experiment number
echo "experiment 4"
echo ""

# Define the directory and file constants
DIR="contrib/romam/NSDI2025/exp4/"
FILE="sinked-packet.delay"
PROTOCOL="dgr"

cp "${DIR}code/${PROTOCOL}.cc" "scratch/${PROTOCOL}-exp4.cc"

# abilene 0 --> 10
TOPO="abilene"
SINK="10"
SENDER="0"
BACK_SINK="5"
BACK_SENDER="2"
./ns3 run "scratch/${PROTOCOL}-exp4.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --backSender=${BACK_SENDER} --backSink=${BACK_SINK}"
cp $FILE "${DIR}results/${PROTOCOL}-${TOPO}.txt"

# att 0 --> 17
TOPO="att"
SINK="17"
SENDER="0"
BACK_SINK="6"
BACK_SENDER="2"
./ns3 run "scratch/${PROTOCOL}-exp4.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --backSender=${BACK_SENDER} --backSink=${BACK_SINK}"
cp $FILE "${DIR}results/${PROTOCOL}-${TOPO}.txt"


# cernet 7 --> 15
TOPO="cernet"
SINK="15"
SENDER="7"
BACK_SINK="12"
BACK_SENDER="2"
./ns3 run "scratch/${PROTOCOL}-exp4.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --backSender=${BACK_SENDER} --backSink=${BACK_SINK}"
cp $FILE "${DIR}results/${PROTOCOL}-${TOPO}.txt"


# geant 14 --> 16
TOPO="geant"
SINK="16"
SENDER="14"
BACK_SINK="0"
BACK_SENDER="3"
./ns3 run "scratch/${PROTOCOL}-exp4.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --backSender=${BACK_SENDER} --backSink=${BACK_SINK}"
cp $FILE "${DIR}results/${PROTOCOL}-${TOPO}.txt"