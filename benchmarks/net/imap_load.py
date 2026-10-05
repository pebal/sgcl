# The module's IMAP server under Python's imaplib as a load generator (Go's
# standard library has no IMAP to compare with): C connections at once,
# each logging in, selecting INBOX and running NOOPs, UID FETCH of flags
# and UID FETCH of whole messages in turn for S seconds; prints the
# commands per second and the FETCH throughput the server gave.
#   build-release/benchmarks/bench_imap server &      (prints "port N")
#   python3 benchmarks/net/imap_load.py 127.0.0.1:N [connections=8] [seconds=5]
import imaplib
import sys
import threading
import time

host, port = sys.argv[1].rsplit(":", 1)
connections = int(sys.argv[2]) if len(sys.argv) > 2 else 8
seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0
lock = threading.Lock()
totals = {"commands": 0, "messages": 0, "bytes": 0}


def worker():
    m = imaplib.IMAP4(host, int(port))
    m.login("bench", "bench")
    m.select("INBOX")
    commands = messages = size = 0
    end = time.time() + seconds
    while time.time() < end:
        m.noop()
        typ, data = m.uid("FETCH", "1:100", "(FLAGS)")
        messages += len(data)
        typ, data = m.uid("FETCH", "1:20", "(BODY.PEEK[])")
        for item in data:
            if isinstance(item, tuple):
                size += len(item[1])
                messages += 1
        commands += 3
    m.logout()
    with lock:
        totals["commands"] += commands
        totals["messages"] += messages
        totals["bytes"] += size


threads = [threading.Thread(target=worker) for _ in range(connections)]
t0 = time.time()
for t in threads:
    t.start()
for t in threads:
    t.join()
wall = time.time() - t0
print("imap_load connections=%d commands/s=%.0f messages/s=%.0f MB/s=%.1f wall=%.2fs" % (
    connections, totals["commands"] / wall, totals["messages"] / wall, totals["bytes"] / wall / 1e6, wall))
