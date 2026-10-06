#!/usr/bin/env python3
# Python's poplib against net::pop3's server, for tests/net/pop3/client.cpp:
# STLS with the tree's test CA, USER/PASS, STAT, LIST, UIDL, RETR, TOP,
# CAPA, DELE, QUIT; then APOP in a second session. Prints one line per step.
#
#   session.py PORT CA.pem
import poplib
import ssl
import sys

port, ca = int(sys.argv[1]), sys.argv[2]
ctx = ssl.create_default_context(cafile=ca)
ctx.check_hostname = False

p = poplib.POP3("127.0.0.1", port, timeout=10)
p.stls(context=ctx)
p.user("alice")
p.pass_("secret")
count, size = p.stat()
print("stat", count, size)
resp, lines, _ = p.list()
print("list", len(lines))
resp, lines, _ = p.uidl()
print("uidl", len(lines))
resp, lines, _ = p.retr(1)
print("retr", "ok" if lines[-1] == b".hidden dot" and lines[0].startswith(b"From: Bob") else lines)
resp, lines, _ = p.top(2, 0)
print("top", "ok" if lines == [b"From: Carol <carol@example.com>", b"Subject: Report", b""] else lines)
caps = p.capa()
print("capa", "ok" if "UIDL" in caps and "TOP" in caps else caps)
p.dele(1)
print("quit", "ok" if p.quit().startswith(b"+OK") else "bad")

q = poplib.POP3("127.0.0.1", port, timeout=10)
q.stls(context=ctx)
print("apop", "ok" if q.apop("alice", "secret").startswith(b"+OK") else "bad")
print("after", q.stat()[0])
q.quit()
