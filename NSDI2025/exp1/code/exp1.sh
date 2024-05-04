
#!/bin/bash
# exp 1
echo "experiment 1"
echo ""

DIR="contrib/romam/NSDI2025/exp1/"

# OSPF
PROTCOL="ospf"
echo "${PROTOCOL}"
cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp1.cc"
TOPO="abilene"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="att"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="cernet"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="geant"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"

# DGR
PROTCOL="dgr"
echo "${PROTOCOL}"
cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp1.cc"
TOPO="abilene"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="att"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="cernet"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="geant"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"


# DDR
PROTCOL="ddr"
echo "${PROTOCOL}"
cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp1.cc"
TOPO="abilene"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="att"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="cernet"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="geant"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"


# kshortest
PROTCOL="kshortest"
echo "${PROTOCOL}"
cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp1.cc"
TOPO="abilene"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="att"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="cernet"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="geant"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"


# Octopus
PROTCOL="octopus"
echo "${PROTOCOL}"
cp "${DIR}code/${PROTCOL}.cc" "scratch/${PROTCOL}-exp1.cc"
TOPO="abilene"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="att"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="cernet"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
TOPO="geant"
./ns3 run "scratch/${PROTCOL}-exp1.cc --topo=${TOPO}"
