#!/bin/bash

# Variables
DEVICE="/dev/vdb"                  # Replace with your device
MOUNT_POINT="/mnt/test"            # Mount point
MODULE="ext2-lite"                 # Kernel module name
IMAGE_FILE="ext2lite.img"          # Disk image (for reference)

if [ "$(id -u)" -ne 0 ]; then
	echo "Error: You must be root."
	exit 1
fi

# Load the ext2-lite module
load_mod() {
	if ! lsmod | grep -q "$MODULE"; then
		echo "Loading $MODULE module..."
		insmod ${MODULE}.ko
		if [ $? -ne 0 ]; then
			echo "Failed to load module $MODULE"
			exit 1
		fi
	else
		echo "Module $MODULE is already loaded."
	fi
}

# Unload the ext2-lite module
unload_mod() {
	echo "Unloading $MODULE module..."
	rmmod $MODULE
	if [ $? -ne 0 ]; then
		echo "Failed to unload module $MODULE"
		exit 1
	fi
	echo "$MODULE module unloaded successfully."
}

# Mount the filesystem
mount() {
	if [ ! -d "$MOUNT_POINT" ]; then
		echo "Creating mount point $MOUNT_POINT..."
		mkdir -p $MOUNT_POINT
	fi

	echo "Mounting $DEVICE to $MOUNT_POINT..."
	mount -t ext2-lite $DEVICE $MOUNT_POINT
	if [ $? -ne 0 ]; then
		echo "Failed to mount $DEVICE"
		exit 1
	fi
	echo "Filesystem mounted successfully!"
}

# Clean the filesystem
clean() {
	echo "Cleaning up $MOUNT_POINT..."
	rm -rf $MOUNT_POINT/*
	echo "Filesystem cleaned."
}

# Unmount the filesystem
unmount() {
	echo "Unmounting $MOUNT_POINT..."
	umount $MOUNT_POINT
	if [ $? -ne 0 ]; then
		echo "Failed to unmount $MOUNT_POINT"
		exit 1
	fi
	echo "Filesystem unmounted successfully."
}

# Run all steps: load, mount, clean, unmount, unload
run_all() {
	load_mod
	mount
	clean
	unmount
	unload_mod
}

# Display usage instructions
usage() {
	echo "Usage: $0 {load|unload|mount|unmount|clean|all}"
	exit 1
}

# Main script execution
if [ $# -eq 0 ]; then
	usage
fi

case "$1" in
	load)
		load_mod
		;;
	unload)
		unload_mod
		;;
	mount)
		mount
		;;
	unmount)
		unmount
		;;
	clean)
		clean
		;;
	all)
		run_all
		;;
	*)
		usage
		;;
esac

