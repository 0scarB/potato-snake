#!/bin/sh

set -eu

for pid in $( ps -C python3 -o pid= ); do
    ps_line_with_cmd="$( ps -o pid=,cmd= $pid )"
    if [ "$( echo "$ps_line_with_cmd" | grep http.server )" != "" ]; then
        kill -9 $pid
        echo "Killed process: $ps_line_with_cmd"
    fi
done

