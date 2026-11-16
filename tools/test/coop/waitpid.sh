#!/bin/bash
# waits until a process ends: waitpid.sh <pid> [secs]; 0 ended, 1 still runs
end=$((SECONDS + ${2:-30}))
while kill -0 "$1" 2>/dev/null; do
	[ $SECONDS -ge $end ] && exit 1
	timeout 0.3 tail -f /dev/null
done
exit 0
