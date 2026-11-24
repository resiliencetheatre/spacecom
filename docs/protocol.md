# Spacecom delivery protocol v1

Spacecom is a two-peer, bidirectional, queued file-delivery service.  It is not
a directory synchronizer: deletes and changes made after a file is queued are
not propagated.

## Topology and ports

Each daemon has one NORM sender aimed at `peer_address:peer_rx_port` and one
receiver bound to `local_rx_port`.  A deployment must allocate a different
receive port to each direction.  With A receiving on 6003 and B on 6005:

* A sets `local_rx_port=6003` and `peer_rx_port=6005`.
* B sets `local_rx_port=6005` and `peer_rx_port=6003`.

The daemons address the peers by their WireGuard addresses.  WireGuard is the
security boundary and provides encryption and peer authentication.  The
application still treats all received metadata as untrusted.

Node IDs are stable, non-zero, distinct 32-bit integers.  The configured
`peer_node_id` is the NORM watermark ACK target.

## Object metadata

Every NORM file object's info field is UTF-8 text with tab-separated fields.
Fields may not contain tabs, CR, LF, NUL, `/`, or `\\` unless noted.

Data object:

```
SC1\tFILE\t<transfer-id>\t<size>\t<sha256-hex>\t<mode-octal>\t<mtime>\t<name>
```

Receipt object (the file payload is a single newline byte; its contents are ignored):

```
SC1\tRECEIPT\t<transfer-id>\tOK
SC1\tRECEIPT\t<transfer-id>\tERROR\t<reason>
```

Transfer IDs are 32 lowercase hexadecimal characters.  SHA-256 values are 64
lowercase hexadecimal characters.  Version 1 deliberately supports a single
safe basename, not a relative directory tree.

## Sender state

Files discovered in `outbox` are copied to immutable files under
`state_dir/objects` and recorded in SQLite before transmission.  States are:

```
queued -> sending -> transport_acked -> complete
                    \-> queued (timeout/restart/error)
```

`NORM_TX_WATERMARK_COMPLETED` establishes only `transport_acked`.  The object
remains durable until an `OK` receipt arrives.  Retry uses bounded exponential
backoff.  Transfer IDs make sends and receipts idempotent.

## Receiver commit

The receiver validates metadata and redirects the NORM cache object to a
unique file below `state_dir/incoming`.  On completion it checks size and
SHA-256, fsyncs the file, and atomically renames it into `inbox`.  It then
fsyncs the inbox directory, records the transfer ID, and queues an `OK`
receipt.  A duplicate ID is acknowledged without overwriting the installed
file.  Invalid metadata, digest failures, and destination collisions generate
an `ERROR` receipt.

Destination names reject absolute paths, dot components, separators, control
characters, and names longer than 255 bytes.  Version 1 never overwrites an
existing destination; a collision is an error.

## Crash and duplicate semantics

SQLite uses WAL and full synchronous mode.  On daemon startup, `sending` and
`transport_acked` rows become `queued`. Opening the database for `spacecomctl list`
does not perform this recovery or change active transfer states. Stale incoming partial files may be
discarded because NORM v1 transfers a whole file per object.  A committed row
is the durable duplicate detector.  Chunk-level cross-restart resume is
reserved for protocol v2.

## Configuration contract

Configuration is `key=value`, one setting per line.  Unknown keys and invalid
values are fatal.  Required keys are `node_id`, `peer_node_id`, `peer_address`,
`local_rx_port`, `peer_rx_port`, `state_dir`, `outbox`, and `inbox`.
Operational tuning includes `tx_rate_bps`, `segment_size`, `block_size`,
`parity_count`, `tx_buffer_bytes`, `rx_buffer_bytes`, `scan_interval_seconds`,
and `retry_seconds`.

