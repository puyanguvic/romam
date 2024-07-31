
#!/bin/bash
# exp 2
echo "experiment 2"
echo ""

DIR="contrib/romam/NSDI2025/exp2/"
FILE="sinked-packet.delay"

# # OSPF
# PROTCOL="ospf"
# echo "${PROTOCOL}"
# cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp2.cc"
# TOPO="abilene"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
# TOPO="att"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
# TOPO="cernet"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
# TOPO="geant"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"

# # DDR
# PROTCOL="ddr"
# echo "${PROTOCOL}"
# cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp2.cc"
# TOPO="abilene"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
# TOPO="att"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
# TOPO="cernet"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
# TOPO="geant"
# ./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
# cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"



# Octopus
PROTCOL="octopus"
echo "${PROTOCOL}"
cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp2.cc"
TOPO="abilene"
./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
TOPO="att"
./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
TOPO="cernet"
./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"
TOPO="geant"
./ns3 run "scratch/${PROTCOL}-exp2.cc --topo=${TOPO}"
cp $FILE "${DIR}results/${PROTCOL}-${TOPO}.txt"