#!/usr/bin/env python3
# MQTT packet by packet against net::mqtt's broker, for
# tests/net/mqtt/interop.cpp: the packets written by hand from the OASIS
# specifications of MQTT 5.0 and 3.1.1 (not from the module), the broker's
# answers checked byte by byte. Prints one line per check.
#
#   conformance.py PORT
import socket
import struct
import sys
import time

PORT = int(sys.argv[1])


def varint(n):
    out = b""
    while True:
        b = n & 0x7F
        n >>= 7
        out += bytes([b | (0x80 if n else 0)])
        if not n:
            return out


def s(text):
    data = text.encode() if isinstance(text, str) else text
    return struct.pack(">H", len(data)) + data


def packet(first, body):
    return bytes([first]) + varint(len(body)) + body


def connect(level, client_id, keep_alive=60, clean=True, props=b""):
    body = s("MQTT") + bytes([level, 0x02 if clean else 0]) + struct.pack(">H", keep_alive)
    if level == 5:
        body += varint(len(props)) + props
    body += s(client_id)
    return packet(0x10, body)


def read_packet(sock):
    first = sock.recv(1)
    if not first:
        return None, None
    length, shift = 0, 0
    while True:
        b = sock.recv(1)[0]
        length |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            break
    body = b""
    while len(body) < length:
        chunk = sock.recv(length - len(body))
        if not chunk:
            break
        body += chunk
    return first[0], body


def dial():
    sock = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    return sock


def check(name, ok, detail=""):
    print(name, "ok" if ok else "FAILED " + repr(detail))


# --- MQTT 5: CONNECT, CONNACK
a = dial()
a.sendall(connect(5, "py-a"))
t, body = read_packet(a)
check("v5 connack", t == 0x20 and body[0] == 0 and body[1] == 0, (t, body))

# SUBSCRIBE (id 1, "py/#", QoS 2), SUBACK granting 2
a.sendall(packet(0x82, struct.pack(">H", 1) + b"\x00" + s("py/#") + b"\x02"))
t, body = read_packet(a)
check("v5 suback", t == 0x90 and body[:2] == b"\x00\x01" and body[-1] == 2, (t, body))

# PUBLISH QoS 2 (id 7): PUBREC, then PUBREL, PUBCOMP. The broker routes the
# message at once (§4.3.3 lets it), so our own copy may come between them
a.sendall(packet(0x34, s("py/x") + struct.pack(">H", 7) + b"\x00" + b"payload"))
got = []
t, body = read_packet(a)
got.append((t, body))
ok = False
pub = None
if t == 0x50 and body[:2] == b"\x00\x07":
    a.sendall(packet(0x62, struct.pack(">H", 7)))
    for _ in range(2):
        tt, bb = read_packet(a)
        got.append((tt, bb))
        if tt == 0x70 and bb[:2] == b"\x00\x07":
            ok = True
        elif tt is not None and (tt & 0xF0) == 0x30:
            pub = (tt, bb)
check("v5 qos2", ok and pub is not None, got)

# the message comes back to us at QoS 2 (we subscribed): our PUBREC, its PUBREL, our PUBCOMP
t, body = pub if pub else (None, b"")
ok = t is not None and (t >> 1) & 3 == 2
tlen = struct.unpack(">H", body[:2])[0]
topic = body[2:2 + tlen].decode()
pid = body[2 + tlen:4 + tlen]
plen_at = 4 + tlen
props_len = body[plen_at]
payload = body[plen_at + 1 + props_len:]
a.sendall(packet(0x50, pid))
t3, body3 = read_packet(a)
a.sendall(packet(0x70, pid))
check("v5 delivery", ok and topic == "py/x" and payload == b"payload" and t3 == 0x62 and body3[:2] == pid, (t, body, t3))

# PINGREQ, PINGRESP
a.sendall(b"\xc0\x00")
t, body = read_packet(a)
check("ping", t == 0xD0 and body == b"", (t, body))
a.sendall(b"\xe0\x00")
a.close()

# --- MQTT 3.1.1: CONNECT, CONNACK of two bytes; a retained message
b = dial()
b.sendall(connect(4, "py-b"))
t, body = read_packet(b)
check("v311 connack", t == 0x20 and body == b"\x00\x00", (t, body))
b.sendall(packet(0x31, s("py311/r") + b"kept"))          # PUBLISH QoS 0 retained
b.sendall(packet(0x82, struct.pack(">H", 2) + s("py311/+") + b"\x00"))
t, body = read_packet(b)                                 # SUBACK
t2, body2 = read_packet(b)                               # the retained PUBLISH, retain flag set
check("v311 retained", t == 0x90 and body == b"\x00\x02\x00" and t2 == 0x31 and body2.endswith(b"kept"), (t, body, t2, body2))
b.close()

# --- a malformed packet: SUBSCRIBE without its reserved flags; DISCONNECT 0x81, then the end
c = dial()
c.sendall(connect(5, "py-c"))
read_packet(c)
c.sendall(packet(0x80, struct.pack(">H", 3) + b"\x00" + s("x") + b"\x00"))
t, body = read_packet(c)
t2, _ = read_packet(c)
check("malformed", t == 0xE0 and body[0] == 0x81 and t2 is None, (t, body, t2))

# --- an unsupported protocol level: CONNACK 0x84 and the end
d = dial()
body = s("MQTT") + bytes([9, 0x02]) + struct.pack(">H", 60) + b"\x00" + s("py-d")
d.sendall(packet(0x10, body))
t, body = read_packet(d)
check("level", t == 0x20 and body[1] == 0x84, (t, body))
d.close()

# --- keep alive 1 s and silence: the broker ends it within 1.5 s (DISCONNECT 0x8D)
e = dial()
e.sendall(connect(5, "py-e", keep_alive=1))
read_packet(e)
start = time.time()
t, body = read_packet(e)
elapsed = time.time() - start
t2, _ = read_packet(e)
check("keepalive", t == 0xE0 and body[0] == 0x8D and 1.0 <= elapsed < 3.0 and t2 is None, (t, body, elapsed))

# --- a first packet that is no CONNECT: closed without a word
f = dial()
f.sendall(b"\xc0\x00")
t, _ = read_packet(f)
check("first packet", t is None, t)
