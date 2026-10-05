# The oracle of the encoding::email tests: Python 3's email package.
#
#   oracle.py summary FILE...   one JSON line per message: what a reader takes
#                               out of it (the subject, the addresses, the
#                               date, the text and HTML bodies, the
#                               attachments with their sizes and hashes)
#   oracle.py build DIR         messages built by Python in DIR (NN.eml), and
#                               DIR/summary.jsonl with their summaries
#
# The hash is FNV-1a 64 of the bytes, in hexadecimal, as the tests compute it.
import email
import json
import os
import sys
from email import policy
from email.message import EmailMessage
from email.utils import format_datetime, make_msgid
import datetime


def fnv(data):
    h = 0xcbf29ce484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return "%016x" % h


def addresses(msg, name):
    h = msg.get(name)
    if h is None:
        return []
    return [[a.display_name, a.addr_spec] for a in h.addresses]


def summary(m):
    out = {}
    out["subject"] = str(m.get("Subject", ""))
    out["from"] = addresses(m, "From")
    out["to"] = addresses(m, "To")
    out["cc"] = addresses(m, "Cc")
    d = m.get("Date")
    out["date"] = int(d.datetime.timestamp()) if d is not None and d.datetime is not None else 0
    out["message_id"] = str(m.get("Message-ID", "")).strip()
    body = m.get_body(preferencelist=("plain",))
    out["text"] = body.get_content() if body is not None else ""
    html = m.get_body(preferencelist=("html",))
    out["html"] = html.get_content() if html is not None else ""
    atts = []
    for a in m.iter_attachments():
        if a.get_content_type() == "message/rfc822":
            payload = a.get_payload(0).as_bytes(policy=policy.SMTP)
            atts.append([a.get_filename() or "", a.get_content_type(), -1, ""])
            continue
        payload = a.get_payload(decode=True) or b""
        atts.append([a.get_filename() or "", a.get_content_type(), len(payload), fnv(payload)])
    out["attachments"] = atts
    return out


def build(directory):
    msgs = []

    m = EmailMessage()
    m["From"] = "Alice Example <alice@example.com>"
    m["To"] = "Bob <bob@example.org>, carol@example.net"
    m["Subject"] = "Plain ASCII"
    m["Date"] = format_datetime(datetime.datetime(2026, 10, 5, 12, 30, 0, tzinfo=datetime.timezone.utc))
    m["Message-ID"] = "<one@example.com>"
    m.set_content("Hello, Bob.\nA second line.\n")
    msgs.append(m)

    m = EmailMessage()
    m["From"] = "Łucja Żółć <lucja@example.pl>"
    m["To"] = "\"Doe, John\" <john@example.com>"
    m["Cc"] = "Zoë <zoe@example.fr>"
    m["Subject"] = "Zażółć gęślą jaźń — the encoded subject goes on long enough to be folded by Python"
    m["Date"] = "Mon, 05 Oct 2026 14:30:00 +0200"
    m.set_content("Treść po polsku: zażółć gęślą jaźń.\n")
    m.add_alternative("<p>Treść <b>po polsku</b></p>\n", subtype="html")
    msgs.append(m)

    m = EmailMessage()
    m["From"] = "sender@example.com"
    m["To"] = "rcpt@example.com"
    m["Subject"] = "Attachments"
    m.set_content("See the files.\n")
    m.add_attachment(bytes(range(256)) * 40, maintype="application", subtype="octet-stream", filename="data.bin")
    m.add_attachment("zażółć\n".encode("utf-8"), maintype="text", subtype="plain", filename="ąę notatka.txt")
    m.add_attachment(b"%PDF-1.4 fake", maintype="application", subtype="pdf", filename="a very long file name that will need the continuations of RFC 2231 to fit — ąęść.pdf")
    msgs.append(m)

    m = EmailMessage()
    m["From"] = "a@example.com"
    m["To"] = "b@example.com"
    m["Subject"] = "Latin-1 body"
    m.set_content("Café crème, naïve façade.\n", charset="iso-8859-1")
    msgs.append(m)

    m = EmailMessage()
    m["From"] = "a@example.com"
    m["To"] = "b@example.com"
    m["Subject"] = "Long lines"
    m.set_content(("x" * 1200) + "\n" + ("ą" * 500) + "\n")
    msgs.append(m)

    m = EmailMessage()
    m["From"] = "a@example.com"
    m["To"] = "b@example.com"
    m["Subject"] = "Related"
    m.set_content("text version\n")
    m.add_alternative("<p><img src=\"cid:logo\"></p>\n", subtype="html")
    m.get_payload()[1].add_related(b"\x89PNG fake image", maintype="image", subtype="png", cid="<logo>")
    m.add_attachment(b"zip bytes", maintype="application", subtype="zip", filename="x.zip")
    msgs.append(m)

    inner = EmailMessage()
    inner["From"] = "inner@example.com"
    inner["To"] = "outer@example.com"
    inner["Subject"] = "The forwarded one"
    inner.set_content("Inner body.\n")
    m = EmailMessage()
    m["From"] = "a@example.com"
    m["To"] = "b@example.com"
    m["Subject"] = "Forward"
    m.set_content("Forwarding.\n")
    m.add_attachment(inner)
    msgs.append(m)

    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, "summary.jsonl"), "w") as s:
        for i, m in enumerate(msgs):
            data = m.as_bytes(policy=policy.SMTP)
            with open(os.path.join(directory, "%02d.eml" % i), "wb") as f:
                f.write(data)
            back = email.message_from_bytes(data, policy=policy.default)
            s.write(json.dumps(summary(back), ensure_ascii=False) + "\n")


if __name__ == "__main__":
    if sys.argv[1] == "summary":
        for path in sys.argv[2:]:
            with open(path, "rb") as f:
                m = email.message_from_bytes(f.read(), policy=policy.default)
            print(json.dumps(summary(m), ensure_ascii=False))
    elif sys.argv[1] == "build":
        build(sys.argv[2])
