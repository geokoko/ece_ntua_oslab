#!/bin/bash

for fd in {3..255}; do
    eval "exec $fd<&-"
    eval "exec $fd>&-"
done


# rmmod ./lunix.ko
rmmod --force lunix.ko

dmesg -C

cd /home/user/shared/oslab/lunix-tng

make clean 

make

insmod ./lunix.ko

./mk-lunix-devs.sh

# Run the command in the background
./lunix-attach /dev/ttyS1 &
# Store the Process ID (PID) of the background command
# PID=$!

# Wait for 1 second
# sleep 1

# Kill the process
# kill $PID

echo ""
echo "dmesg:"
dmesg -w | grep "chrdev"
