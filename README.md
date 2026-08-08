# spacecom

Tiny standalone NORM-based file transfer tools for Debian/amd64 hosts using the
host-installed `libnorm-dev` package.

![Intro picture](img/spacecom.excalidraw.png "Introduction")


This project contains:

- `spacecom-send` — send one file with NORM
- `spacecom-recv` — receive one file with NORM into a cache/output directory
- `spacecomd` — durable bidirectional queued delivery daemon
- `spacecomctl` — queue files and inspect outbound state

The original send/receive programs remain useful as NORM diagnostics.  New
deployments should use the daemon.

## Target environment

Example package:

```text
libnorm-dev/stable 1.5.9+dfsg-3.1+b2 amd64
```

## Build

```sh
make
```

Build dependencies on Debian are `g++`, `make`, `libnorm-dev`,
`libsqlite3-dev`, and `libssl-dev`.

If pkg-config metadata is unavailable:

```sh
make NORM_CFLAGS='-I/usr/include' NORM_LIBS='-lnorm -lpthread'
```

## Unicast usage

Receiver host `10.0.0.4`:

```sh
mkdir -p /tmp/norm-rx
./spacecom-recv addr 10.0.0.10/6003 /tmp/norm-rx
```

Sender host `10.0.0.10`:

```sh
./spacecom-send addr 10.0.0.4/6003 /path/to/file
```

For unicast, sender uses the receiver IP and receiver uses the sender IP.

## Notes

- Sender segment size defaults to 1000 bytes.
- The diagnostic receiver remains active for subsequent transfers.
- The diagnostic tools are exploration utilities, not hardened production
  software.

## Bidirectional daemon

Copy `spacecom.conf.example` to each peer and give each direction a distinct
receive port.  Endpoint A might receive on 6003 and send to 6005; endpoint B
must receive on 6005 and send to 6003.  Use stable, distinct NORM node IDs and
the peers' WireGuard IP addresses.

Start a daemon in the foreground:

```sh
spacecomd /etc/spacecom.conf
```

Queue and inspect files:

```sh
spacecomctl /etc/spacecom.conf send ./report.tar
spacecomctl /etc/spacecom.conf list
```

The daemon copies newly queued files into immutable storage, transfers them
with NORM, verifies SHA-256 at the receiver, commits without overwriting an
existing file, and retains the sender copy until an application-level receipt
returns.  State survives daemon restarts in SQLite.

See [docs/protocol.md](docs/protocol.md) for the protocol, trust boundary,
state transitions, and v1 limitations.  A sample hardened unit is available
at `packaging/spacecomd.service`; its writable paths must match the local
configuration.
