#!/bin/sh

set -eu

HOST=localhost
PORT=8080
log_file="http-server.log"
echo "Starting development HTTP server in background."
echo "Logging to $log_file."
echo "Open http://$HOST:$PORT"
python3 -m http.server -b localhost 8080 1>$log_file 2>$log_file &
