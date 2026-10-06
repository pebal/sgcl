# An LSP-style JSON-RPC 2.0 peer written with Python's standard library
# alone (json, socket): it connects to the test's listener, frames by
# Content-Length, answers "echo" and "add", calls the C++ side's "upper"
# while serving "ask_upper", and sends a notification "log". An
# implementation of its own of the framing and the message rules, against
# which net::jsonrpc::peer is checked.
import json
import socket
import sys

port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port), timeout=10)
buf = b""


def read():
    global buf
    while b"\r\n\r\n" not in buf:
        part = s.recv(65536)
        if not part:
            return None
        buf += part
    head, _, rest = buf.partition(b"\r\n\r\n")
    length = None
    for line in head.split(b"\r\n"):
        name, _, value = line.partition(b":")
        if name.strip().lower() == b"content-length":
            length = int(value.strip())
    while len(rest) < length:
        part = s.recv(65536)
        if not part:
            return None
        rest += part
    buf = rest[length:]
    return json.loads(rest[:length].decode("utf-8"))


def write(msg):
    body = json.dumps(msg).encode("utf-8")
    s.sendall(b"Content-Length: " + str(len(body)).encode() + b"\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n" + body)


next_id = 1000
while True:
    msg = read()
    if msg is None:
        break
    if isinstance(msg, list):
        out = [{"jsonrpc": "2.0", "id": m["id"], "result": m["params"]} for m in msg if "id" in m]
        write(out)
        continue
    method = msg.get("method")
    if method == "echo":
        write({"jsonrpc": "2.0", "id": msg["id"], "result": msg["params"]})
    elif method == "add":
        a, b = msg["params"]
        write({"jsonrpc": "2.0", "id": msg["id"], "result": a + b})
    elif method == "ask_upper":
        next_id += 1
        write({"jsonrpc": "2.0", "id": next_id, "method": "upper", "params": {"text": msg["params"]["text"]}})
        reply = read()
        while "method" in reply:
            reply = read()
        write({"jsonrpc": "2.0", "id": msg["id"], "result": reply["result"]})
    elif method == "say":
        write({"jsonrpc": "2.0", "method": "log", "params": {"message": msg["params"]["text"]}})
        write({"jsonrpc": "2.0", "id": msg["id"], "result": True})
    elif method == "exit":
        write({"jsonrpc": "2.0", "id": msg["id"], "result": None})
        break
    elif "id" in msg:
        write({"jsonrpc": "2.0", "id": msg["id"], "error": {"code": -32601, "message": "Method not found", "data": method}})
s.close()
print("ok")
