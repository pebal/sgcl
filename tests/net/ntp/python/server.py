# An SNTP server written with Python's standard library alone (socket,
# struct, time): RFC 4330's answer by hand, its clock offset by the seconds
# given, so that the module's client is checked against an implementation
# of its own. Prints its port, answers one query, exits.
import socket
import struct
import sys
import time

offset = float(sys.argv[1])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1], flush=True)
s.settimeout(10)
data, addr = s.recvfrom(1024)
recv = time.time() + offset


def ntp(t):
    t += 2208988800
    return (int(t) << 32) | int((t - int(t)) * (1 << 32))


version = (data[0] >> 3) & 7
originate = data[40:48]
reply = struct.pack("!BBbbII4sQ8sQQ", (0 << 6) | (version << 3) | 4, 2, 6, -20, 0x00000100, 0x00000200, bytes([192, 0, 2, 1]),
                    ntp(recv - 30), originate, ntp(recv), ntp(time.time() + offset))
s.sendto(reply, addr)
