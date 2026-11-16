#!/bin/bash
# waits until a file has a line matching a pattern: wait.sh <file> <regex> [secs]; 0 found, 1 timeout
f=$1 re=$2 secs=${3:-60}
end=$((SECONDS + secs))
while [ $SECONDS -lt $end ]; do
	grep -qE "$re" "$f" 2>/dev/null && exit 0
	timeout 0.5 tail -f /dev/null
done
exit 1
