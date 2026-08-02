#!/bin/bash
# user is playing now: record 3s directly from gadget, measure real rate
timeout 8 arecord -D hw:1,0 -f S16_LE -r 48000 -c 2 -d 3 /tmp/rec3s.raw 2>&1 | tail -1
SIZE=$(stat -c %s /tmp/rec3s.raw 2>/dev/null)
echo "recorded $SIZE bytes in 3s"
if [ "$SIZE" -gt 1000 ]; then
  echo "REAL gadget rate: ~$((SIZE/4/3)) Hz (48k expected)"
else
  echo "NO DATA"
fi
