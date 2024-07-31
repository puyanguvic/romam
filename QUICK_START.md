# Quick Start Guide

This guide will help you set up ROMAM on ns-3.40 and run your first simulation.

## Prerequisites

- Ubuntu 20.04 or later (or compatible Linux distribution)
- ns-3.40 (installed and configured)
- C++ compiler supporting C++17 or later
- CMake (version 3.10 or later)
- Git

## Installation Steps

### Step 1: Install ns-3.40

1. Download ns-3.40:
    ```
   wget https://www.nsnam.org/releases/ns-allinone-3.40.tar.bz2
    ```
2. Extract the archive:
    ```
    tar xjf ns-allinone-3.40.tar.bz2
    ```
3. Navigate to the ns-3.40 directory:
  ```
   cd ns-allinone-3.40/ns-3.40
  ```
4. Configure and build ns-3:
   ```
   ./ns3 configure --enable-examples --enable-tests
   ./ns3 build
    ```
### Step 2: Clone ROMAM Repository

1. Clone the ROMAM repository into the `src` directory of ns-3:
```
  cd src
  git clone https://github.com/your-username/ROMAM.git romam
  cd ..
```

### Step 3: Configure and Build ns-3 with ROMAM

1. Reconfigure and build ns-3 to include ROMAM:
```
  ./ns3 configure --enable-examples --enable-tests
  ./ns3 build
```
   
## Running a Simulation

Run a basic ROMAM simulation using one of the provided examples:
```  
  ./ns3 --run "romam-example --protocol=OSPF --topology=Abilene"
```
