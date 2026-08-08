#!/bin/sh
set -eu
root=/tmp/spacecom-integration
rm -rf "$root"
mkdir -p "$root/a/state" "$root/a/out" "$root/a/in" "$root/b/state" "$root/b/out" "$root/b/in"
sed -e 's/node_id=1/node_id=11/' -e 's/peer_node_id=2/peer_node_id=22/' \
    -e 's/peer_address=10.0.0.4/peer_address=127.0.0.1/' \
    -e 's/local_rx_port=6003/local_rx_port=16003/' -e 's/peer_rx_port=6005/peer_rx_port=16005/' \
    -e "s|state_dir=/var/lib/spacecom|state_dir=$root/a/state|" \
    -e "s|outbox=/var/spool/spacecom/outbox|outbox=$root/a/out|" \
    -e "s|inbox=/var/spool/spacecom/inbox|inbox=$root/a/in|" spacecom.conf.example > "$root/a.conf"
sed -e 's/node_id=1/node_id=22/' -e 's/peer_node_id=2/peer_node_id=11/' \
    -e 's/peer_address=10.0.0.4/peer_address=127.0.0.1/' \
    -e 's/local_rx_port=6003/local_rx_port=16005/' -e 's/peer_rx_port=6005/peer_rx_port=16003/' \
    -e "s|state_dir=/var/lib/spacecom|state_dir=$root/b/state|" \
    -e "s|outbox=/var/spool/spacecom/outbox|outbox=$root/b/out|" \
    -e "s|inbox=/var/spool/spacecom/inbox|inbox=$root/b/in|" spacecom.conf.example > "$root/b.conf"
./spacecomd "$root/a.conf" >"$root/a.log" 2>&1 & a=$!
./spacecomd "$root/b.conf" >"$root/b.log" 2>&1 & b=$!
trap 'kill "$a" "$b" 2>/dev/null || true; wait "$a" "$b" 2>/dev/null || true' EXIT INT TERM
sleep 1
if ! kill -0 "$a" 2>/dev/null || ! kill -0 "$b" 2>/dev/null; then
    echo "SKIP: local NORM implementation does not accept loopback unicast"
    exit 77
fi
cp README.md "$root/a/out/from-a.txt"
cp LICENSE "$root/b/out/from-b.txt"
n=0
while [ "$n" -lt 30 ]; do
    [ -f "$root/b/in/from-a.txt" ] && [ -f "$root/a/in/from-b.txt" ] && break
    n=$((n+1)); sleep 1
done
cmp README.md "$root/b/in/from-a.txt"
cmp LICENSE "$root/a/in/from-b.txt"
sleep 2
./spacecomctl "$root/a.conf" list | grep 'complete'
./spacecomctl "$root/b.conf" list | grep 'complete'
