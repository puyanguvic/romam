#!/bin/bash

# Print the experiment number
echo "experiment 3"
echo ""

# Define the directory and file constants
DIR="contrib/romam/NSDI2025/exp3/"
FILE="sinked-packet.delay"

# List of protocols to iterate over
# PROTOCOLS=("ospf" "ddr" "dgr" "kshortest" "octopus")
PROTOCOLS=("kshortest")
# List of topologies
TOPOS=("abilene" "att" "cernet" "geant")

# Loop over each topology
for TOPO in "${TOPOS[@]}"; do
    # Loop over each protocol and perform the actions for each topology
    for PROTOCOL in "${PROTOCOLS[@]}"; do
        cp "${DIR}code/${PROTOCOL}.cc" "scratch/${PROTOCOL}-exp3.cc"
        ./ns3 run "scratch/${PROTOCOL}-exp3.cc --topo=${TOPO}"
        cp $FILE "${DIR}results/${PROTOCOL}-${TOPO}.txt"
    done
done


# # abilene 0 --> 10 
# TOPO="abilene"
# SINK="10"
# SENDER="0"
# BEGIN=19500
# STEP=500
# for i in {1..1}
# do
# TIME=$(($i*$STEP))
# BUDGET=$(($BEGIN+$TIME))
# echo "${TOPO}${DATARATE}"
# ./ns3 run "scratch/routing_kshort.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --budget=${BUDGET}"
# cp $FILE "${DIR}result/${TOPO}/KSHORT-${i}.txt"
# done

# # att 0 --> 17
# TOPO="att"
# SINK="17"
# SENDER="0"
# BEGIN=20000
# STEP=700
# for i in {1..1}
# do
# TIME=$(($i*$STEP))
# BUDGET=$(($BEGIN+$TIME))
# echo "${TOPO}${DATARATE}"
# ./ns3 run "scratch/routing_kshort.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --budget=${BUDGET}"
# cp $FILE "${DIR}result/${TOPO}/KSHORT-${i}.txt"
# done

# # cernet 7 --> 15
# TOPO="cernet"
# SINK="15"
# SENDER="7"
# BEGIN=5000
# STEP=700
# for i in {1..1}
# do
# TIME=$(($i*$STEP))
# BUDGET=$(($BEGIN+$TIME))
# echo "${TOPO}${DATARATE}"
# ./ns3 run "scratch/routing_kshort.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --budget=${BUDGET}"
# cp $FILE "${DIR}result/${TOPO}/KSHORT-${i}.txt"
# done

# # geant 14 --> 16
# TOPO="geant"
# SINK="16"
# SENDER="14"
# BEGIN=20000
# STEP=800
# for i in {1..1}
# do
# TIME=$(($i*$STEP))
# BUDGET=$(($BEGIN+$TIME))
# echo "${TOPO}${DATARATE}"
# ./ns3 run "scratch/routing_kshort.cc --topo=${TOPO} --sink=${SINK} --sender=${SENDER} --budget=${BUDGET}"
# cp $FILE "${DIR}result/${TOPO}/KSHORT-${i}.txt"
# done