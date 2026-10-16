#!/bin/sh
"$@" & pid=$!
while kill -0 $pid 2>/dev/null; do
  a=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
  if [ "$a" -lt 120 ]; then echo "GUARD: MemAvailable ${a}MB, killing $pid"; kill -9 $pid; fi
  sleep 0.2
done
wait $pid; echo "exit $?"
