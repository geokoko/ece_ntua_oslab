./riddle & # Run riddle in the background

pid=$! # Get the PID Of the last process

sleep 2

kill -18 $pid

echo "SIGCONT signal sent to $pid"
