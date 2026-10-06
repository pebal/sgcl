# The other side of tests/net/http/auth.cpp's client cases: a Digest server
# of RFC 7616 written with Python's standard library alone (hashlib), on a
# port of the loopback ("port N" on the first line). /<algorithm>/... asks
# for that algorithm (MD5, MD5-sess, SHA-256, SHA-256-sess, SHA-512-256,
# SHA-512-256-sess) with qop auth and auth-int; /stale/... answers its first
# good request (once) stale=true; /userhash/... asks for userhash; /basic/... asks
# for Basic. The user is "ann" (or "Jäsøn Doe"), the password "secret".
# A good request is answered "ok <user> <the count of 401s so far>";
# /quit ends the program.
import base64, hashlib, http.server, os, socketserver, sys, urllib.parse

USERS = {"ann": "secret", "Jäsøn Doe": "secret"}
REALM = "py@example.org"
STATE = {"401": 0, "stale": set(), "nonces": {}}


def H(alg, data):
    name = {"MD5": "md5", "SHA-256": "sha256", "SHA-512-256": "sha512_256"}[alg]
    return hashlib.new(name, data.encode("utf-8")).hexdigest()


def parse(value):
    scheme, _, rest = value.partition(" ")
    params, i = {}, 0
    while i < len(rest):
        while i < len(rest) and rest[i] in " ,":
            i += 1
        j = rest.find("=", i)
        if j < 0:
            break
        name = rest[i:j].strip().lower()
        i = j + 1
        if i < len(rest) and rest[i] == '"':
            k, out = i + 1, ""
            while k < len(rest) and rest[k] != '"':
                if rest[k] == "\\":
                    k += 1
                out += rest[k]
                k += 1
            params[name] = out
            i = k + 1
        else:
            k = rest.find(",", i)
            k = len(rest) if k < 0 else k
            params[name] = rest[i:k].strip()
            i = k
    return scheme, params


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def answer(self, code, text, extra=()):
        body = text.encode()
        self.send_response(code)
        for k, v in extra:
            self.send_header(k, v)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def challenge(self, alg, stale=False, userhash=False):
        STATE["401"] += 1
        nonce = base64.b64encode(os.urandom(18)).decode()
        STATE["nonces"][nonce] = 0
        c = 'Digest realm="%s", qop="auth, auth-int", algorithm=%s, nonce="%s", opaque="op4que", charset=UTF-8' % (REALM, alg, nonce)
        if stale:
            c += ", stale=true"
        if userhash:
            c += ", userhash=true"
        self.answer(401, "unauthorized", [("WWW-Authenticate", c)])

    def handle_any(self):
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length).decode("utf-8") if length else ""
        parts = self.path.split("/")
        kind = parts[1] if len(parts) > 1 else ""
        if kind == "quit":
            self.answer(200, "bye")
            os._exit(0)
        auth = self.headers.get("Authorization")
        if kind == "basic":
            if auth and auth.startswith("Basic "):
                user, _, pw = base64.b64decode(auth[6:]).decode("utf-8").partition(":")
                if USERS.get(user) == pw:
                    return self.answer(200, "ok %s %d" % (user, STATE["401"]))
            STATE["401"] += 1
            return self.answer(401, "unauthorized", [("WWW-Authenticate", 'Basic realm="%s", charset="UTF-8"' % REALM)])
        stale_mode = kind == "stale"
        userhash = kind == "userhash"
        alg = "SHA-256" if kind in ("stale", "userhash") else kind
        if userhash:
            alg = "SHA-512-256"
        base_alg = alg[:-5] if alg.endswith("-sess") else alg
        if not auth or not auth.startswith("Digest "):
            return self.challenge(alg, userhash=userhash)
        scheme, p = parse(auth)
        nonce = p.get("nonce", "")
        if nonce not in STATE["nonces"] or p.get("realm") != REALM or p.get("algorithm", "MD5") != alg or p.get("opaque") != "op4que":
            return self.challenge(alg, userhash=userhash)
        if p.get("uri") != self.path:
            return self.answer(400, "uri")
        user = None
        if p.get("userhash") == "true":
            for u in USERS:
                if H(base_alg, "%s:%s" % (u, REALM)) == p.get("username"):
                    user = u
        elif "username*" in p:
            enc = p["username*"]
            user = urllib.parse.unquote(enc.split("''", 1)[1], encoding="utf-8")
        else:
            user = p.get("username")
        if user not in USERS:
            return self.challenge(alg, userhash=userhash)
        nc = int(p.get("nc", "0"), 16)
        if nc <= STATE["nonces"][nonce]:
            return self.challenge(alg, userhash=userhash)
        STATE["nonces"][nonce] = nc
        qop = p.get("qop")
        cnonce = p.get("cnonce", "")
        ha1 = H(base_alg, "%s:%s:%s" % (user, REALM, USERS[user]))
        if alg.endswith("-sess"):
            ha1 = H(base_alg, "%s:%s:%s" % (ha1, nonce, cnonce))
        a2 = "%s:%s" % (self.command, p.get("uri"))
        if qop == "auth-int":
            a2 += ":" + H(base_alg, body)
        ha2 = H(base_alg, a2)
        expected = H(base_alg, "%s:%s:%s:%s:%s:%s" % (ha1, nonce, p.get("nc"), cnonce, qop, ha2))
        if expected != p.get("response"):
            return self.challenge(alg, userhash=userhash)
        if stale_mode and not STATE["stale"]:
            STATE["stale"].add(nonce)   # once: the first good answer is told its nonce is stale
            return self.challenge(alg, stale=True)
        self.answer(200, "ok %s %d %s" % (user, STATE["401"], qop))

    do_GET = handle_any
    do_POST = handle_any
    do_PUT = handle_any


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


s = Server(("127.0.0.1", 0), Handler)
print("port %d" % s.server_address[1], flush=True)
s.serve_forever()
