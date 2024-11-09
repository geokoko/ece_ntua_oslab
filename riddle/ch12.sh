#!/bin/bash

# Run strace on riddle and capture both the file name and output in a single run
strace_output=$(strace -e trace=openat,write ./riddle 2>&1 &)

file=$(echo "$strace_output" | grep "/tmp/riddle-" | grep -o '/tmp/riddle-[^"]*')

echo "Captured file: $file"

# Check if the file was found
if [ -z "$file" ]; then
    echo "File not found in strace output, exiting."
    exit 1
fi

# Waiting for file creation to create hard link
while [ ! -f "$file" ]; do
    echo "Waiting for file to be created..."
    sleep 1
done

ln "$file" /tmp/riddle_link

output=$(echo "$strace_output" | grep "I want to find the char")

character=$(echo "$output" | awk -F"'" '{print $2}')
memory_address=$(echo "$output" | grep -o -E '0x[0-9a-fA-F]+')

echo "Starting ch12.c"
echo "Extracted character: $character"
echo "Extracted memory address: $memory_address"

./ch12_ "$memory_address" "$character" /tmp/riddle_link

echo "Full output:"
echo "$output"
echo "Extracted character:"
echo "$character"
echo "Extracted memory address:"
echo "$memory_address"

wait
