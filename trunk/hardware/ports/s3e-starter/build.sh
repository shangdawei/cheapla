#!/bin/sh
. /opt/Xilinx/10.1/ISE/settings32.sh
. /opt/Xilinx/10.1/EDK/settings32.sh
echo "save make; exit" | xps -nw system.xmp
make -f system.make bits program
