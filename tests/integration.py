#!/usr/bin/env python3
"""Exercise real NORM delivery and receipts in both directions on localhost."""
import os
from pathlib import Path
import secrets
import signal
import socket
import sqlite3
import subprocess
import tempfile
import time

SOURCE = Path(__file__).resolve().parent.parent


def ports():
    for _ in range(100):
        base = 20000 + secrets.randbelow(4000) * 10
        sockets = []
        try:
            for port in range(base, base + 4):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sockets.append(sock)
                sock.bind(("127.0.0.1", port))
            return base
        except OSError:
            pass
        finally:
            for sock in sockets:
                sock.close()
    raise RuntimeError("cannot allocate localhost test ports")


def main():
    base = ports()
    with tempfile.TemporaryDirectory(prefix="spacecom-integration-") as folder:
        root = Path(folder)
        processes = []
        logs = []
        try:
            for label, node, peer, rx, tx in [("a", 1, 2, base, base+2),
                                               ("b", 2, 1, base+2, base)]:
                directory = root / label
                directory.mkdir()
                for name in ("state", "out", "in"):
                    (directory / name).mkdir()
                config = directory / "config"
                config.write_text(f"node_id={node}\npeer_node_id={peer}\n"
                                  f"peer_address=127.0.0.1\nlocal_rx_port={rx}\npeer_rx_port={tx}\n"
                                  f"state_dir={directory}/state\noutbox={directory}/out\n"
                                  f"inbox={directory}/in\nscan_interval_seconds=1\nretry_seconds=2\n")
                log = open(directory / "log", "w+")
                logs.append((label, log))
                processes.append(subprocess.Popen([str(SOURCE / "spacecomd"), str(config)],
                                                  stdout=log, stderr=log))
            expected = {}
            for label in ("a", "b"):
                for number in range(3):
                    name = f"from-{label}-{number}.bin"
                    payload = (f"{label}-{number}\n".encode() * (1 + number * 2048))
                    expected[name] = payload
                    (root / label / "out" / name).write_bytes(payload)
            deadline = time.monotonic() + 45
            while True:
                assert all(p.poll() is None for p in processes), "a daemon exited during delivery"
                complete = True
                for label in ("a", "b"):
                    db = root / label / "state/spacecom.db"
                    if not db.exists():
                        complete = False
                        continue
                    with sqlite3.connect(f"file:{db}?mode=ro", uri=True) as conn:
                        states = conn.execute("SELECT state FROM outbound").fetchall()
                    complete &= len(states) == 3 and all(row[0] == "complete" for row in states)
                if complete:
                    break
                assert time.monotonic() < deadline, "timed out waiting for application receipts"
                time.sleep(0.1)
            for label, other in [("a", "b"), ("b", "a")]:
                for number in range(3):
                    name = f"from-{other}-{number}.bin"
                    assert (root / label / "in" / name).read_bytes() == expected[name]
                result = subprocess.run([str(SOURCE / "spacecomctl"), str(root/label/"config"), "list"],
                                        check=True, text=True, capture_output=True)
                assert len(result.stdout.splitlines()) == 3
                assert all("\tcomplete\t" in line for line in result.stdout.splitlines())
            time.sleep(1)
            assert all(p.poll() is None for p in processes), "daemon crashed after receipts"
            print("PASS: six bidirectional files, matching contents, complete receipts, live daemons")
        except BaseException:
            for label, log in logs:
                log.flush()
                log.seek(0)
                print(f"--- daemon {label} ---\n{log.read()}")
            raise
        finally:
            for process in processes:
                if process.poll() is None:
                    process.send_signal(signal.SIGTERM)
            for process in processes:
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            for _, log in logs:
                log.close()


if __name__ == "__main__":
    main()
