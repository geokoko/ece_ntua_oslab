#!/bin/bash

# Ensure the nodes for the first four serial ports are there.

for i in $(seq 0 3); do
	node="/dev/ttyS$i"
	if [[ ! -e $node ]]; then
		mknod $node c 4 $((64 + $i))
		if [[ $? -eq 17 ]]; then
			echo "Error: $node already exists (EEXIST)."
		fi
	else
		echo "$node already exists, skipping..."
	fi
done

# Lunix:TNG nodes: 16 sensors, each has 3 nodes.
for sensor in $(seq 0 15); do
    for type in batt temp light; do
        node="/dev/lunix$sensor-$type"
        case $type in
            batt) offset=0 ;;
            temp) offset=1 ;;
            light) offset=2 ;;
        esac

        if [[ ! -e $node ]]; then
            mknod $node c 60 $((sensor * 8 + offset))
            chmod 666 $node
            echo "Created $node"
        else
            echo "$node already exists, skipping..."
        fi
    done
done

