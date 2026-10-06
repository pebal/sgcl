#!/usr/bin/env python3
# A DKIM verifier written apart from net::dkim, from RFC 6376 and RFC 8463,
# for the tests: Python's own parsing and canonicalization, OpenSSL's
# signature checks (the command line tool given first). It checks the first
# DKIM-Signature of a message against a key record given in a file, and
# prints "pass" or "fail".
#
#   dkim_verify.py OPENSSL MESSAGE.eml RECORD.txt
import base64
import hashlib
import os
import re
import subprocess
import sys
import tempfile


def fields_and_body(data):
    head, sep, body = data.partition(b"\r\n\r\n")
    if not sep:
        head, body = data, b""
        if head.endswith(b"\r\n"):
            head = head[:-2]
    lines = head.split(b"\r\n")
    fields = []
    for line in lines:
        if line[:1] in (b" ", b"\t") and fields:
            fields[-1] += b"\r\n" + line
        else:
            fields.append(line)
    return [f + b"\r\n" for f in fields], body


def tags(value):
    out = {}
    for spec in value.split(";"):
        if not spec.strip():
            continue
        name, _, v = spec.partition("=")
        out[name.strip()] = v.strip()
    return out


def canon_header(field, how):
    if how == "simple":
        return field
    name, _, value = field.partition(b":")
    value = value.replace(b"\r\n", b"")
    value = re.sub(rb"[ \t]+", b" ", value).strip(b" \t")
    return name.strip(b" \t").lower() + b":" + value + b"\r\n"


def canon_body(body, how):
    if how == "relaxed":
        lines = body.split(b"\r\n")
        lines = [re.sub(rb"[ \t]+", b" ", l).rstrip(b" ") for l in lines]
        body = b"\r\n".join(lines)
    while body.endswith(b"\r\n"):
        body = body[:-2]
    if body or how == "simple":
        body += b"\r\n"
    return body


def openssl_verify(openssl, record, algorithm, data, signature):
    with tempfile.TemporaryDirectory() as d:
        key = base64.b64decode(record["p"])
        if algorithm == "ed25519-sha256":
            key = bytes.fromhex("302a300506032b6570032100") + key
        pem = "-----BEGIN PUBLIC KEY-----\n" + base64.encodebytes(key).decode() + "-----END PUBLIC KEY-----\n"
        open(os.path.join(d, "k.pem"), "w").write(pem)
        open(os.path.join(d, "s.bin"), "wb").write(signature)
        if algorithm == "rsa-sha256":
            open(os.path.join(d, "d.bin"), "wb").write(data)
            cmd = [openssl, "dgst", "-sha256", "-verify", "k.pem", "-signature", "s.bin", "d.bin"]
        else:
            open(os.path.join(d, "d.bin"), "wb").write(hashlib.sha256(data).digest())
            cmd = [openssl, "pkeyutl", "-verify", "-pubin", "-inkey", "k.pem", "-rawin", "-in", "d.bin", "-sigfile", "s.bin"]
        return subprocess.run(cmd, cwd=d, capture_output=True).returncode == 0


def main():
    openssl, message, record_file = sys.argv[1:4]
    data = open(message, "rb").read()
    record = tags(open(record_file).read())
    fields, body = fields_and_body(data)
    own = next(f for f in fields if f.split(b":", 1)[0].strip().lower() == b"dkim-signature")
    sig = tags(own.split(b":", 1)[1].decode())
    head_how, _, body_how = sig.get("c", "simple/simple").partition("/")
    body_how = body_how or "simple"
    canon = canon_body(body, body_how)
    if "l" in sig:
        canon = canon[: int(sig["l"])]
    if base64.b64decode(re.sub(r"\s", "", sig["bh"])) != hashlib.sha256(canon).digest():
        print("fail")
        return
    used = {}
    data_to_sign = b""
    for name in [n.strip().lower() for n in sig["h"].split(":")]:
        candidates = [i for i, f in enumerate(fields) if f.split(b":", 1)[0].strip().lower().decode() == name]
        taken = used.get(name, len(fields))
        below = [i for i in candidates if i < taken]
        if below:
            data_to_sign += canon_header(fields[below[-1]], head_how)
            used[name] = below[-1]
        else:
            used[name] = -1
    without_b = re.sub(rb"(;\s*b=)[^;]*", rb"\1", own, count=1)
    if without_b == own:
        without_b = re.sub(rb"(:\s*b=)[^;]*", rb"\1", own, count=1)
    if not without_b.endswith(b"\r\n"):
        without_b += b"\r\n"
    data_to_sign += canon_header(without_b, head_how)[:-2]
    signature = base64.b64decode(re.sub(r"\s", "", sig["b"]))
    print("pass" if openssl_verify(openssl, record, sig["a"], data_to_sign, signature) else "fail")


main()
