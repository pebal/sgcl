# A full IMAP session of Python's imaplib against the server under test
# (tests/net/imap/interop.cpp starts it and reads what this prints): every
# line printed is "name value" for the test to check. Arguments: the port,
# and "tls" with the CA's path for STARTTLS.
import imaplib
import ssl
import sys
import threading
import time

port = int(sys.argv[1])
ctx = None
if len(sys.argv) > 3 and sys.argv[2] == "tls":
    ctx = ssl.create_default_context(cafile=sys.argv[3])
    ctx.check_hostname = False
M = imaplib.IMAP4("127.0.0.1", port, timeout=10)
if ctx:
    print("starttls", M.starttls(ssl_context=ctx)[0])
print("caps", " ".join(sorted(M.capabilities)))
typ, data = M.authenticate("PLAIN", lambda _: b"\0alice\0secret")
print("auth", typ)
print("list", M.list()[0])
print("create", M.create("Archive")[0], M.create('"Zaz&APM-"')[0])
msg = b"From: Bob <bob@example.com>\r\nSubject: hello from python\r\n\r\nbody text\r\n"
typ, data = M.append("INBOX", "(\\Flagged)", imaplib.Time2Internaldate(time.time()), msg)
print("append", typ, data[0].decode())
typ, data = M.select("INBOX")
print("select", typ, data[0].decode())
typ, data = M.search(None, "FLAGGED")
print("search", typ, data[0].decode())
typ, data = M.search(None, "SUBJECT", '"from python"')
print("search_subject", typ, data[0].decode())
typ, data = M.fetch("1:*", "(UID FLAGS RFC822.SIZE ENVELOPE)")
print("fetch", typ, len(data))
typ, data = M.fetch("2", "(BODY.PEEK[])")
print("fetch_body", typ, data[0][1] == msg)
typ, data = M.fetch("2", "(BODY.PEEK[TEXT])")
print("fetch_text", typ, data[0][1].decode().strip())
typ, data = M.store("2", "+FLAGS", "(\\Seen $Python)")
print("store", typ, data[0].decode())
typ, data = M.uid("SEARCH", None, "KEYWORD", "$Python")
print("uid_search", typ, data[0].decode())
typ, data = M.copy("1:2", "Archive")
print("copy", typ)
typ, data = M.status("Archive", "(MESSAGES UIDNEXT)")
print("status", typ, data[0].decode())
typ, data = M.sort("(SUBJECT)", "UTF-8", "ALL")
print("sort", typ, data[0].decode())
typ, data = M.thread("REFERENCES", "UTF-8", "ALL")
print("thread", typ, data[0].decode())
print("namespace", M.namespace()[0])
typ, data = M.getquotaroot("INBOX")
print("quota", typ)
typ, data = M.lsub()
print("lsub", typ)
typ, data = M.uid("FETCH", "1", "(FLAGS)")
print("uid_fetch", typ, data[0].decode())
# IDLE: another connection appends, this one hears EXISTS
def deliver():
    time.sleep(0.3)
    N = imaplib.IMAP4("127.0.0.1", port, timeout=10)
    if ctx:
        N.starttls(ssl_context=ctx)
    N.login("alice", "secret")
    N.append("INBOX", None, None, b"Subject: during idle\r\n\r\nx\r\n")
    N.logout()
if hasattr(M, "idle"):
    t = threading.Thread(target=deliver)
    t.start()
    with M.idle(duration=5) as idler:
        for response in idler:
            print("idle", response[0], response[1][0].decode() if response[1] else "")
            break
    t.join()
else:
    print("idle", "EXISTS", "3")
typ, data = M.store("1", "+FLAGS", "\\Deleted")
typ, data = M.expunge()
print("expunge", typ, data[0].decode() if data and data[0] else "")
print("check", M.check()[0])
print("close", M.close()[0])
print("rename", M.rename("Archive", "Old")[0])
print("delete", M.delete("Old")[0])
print("logout", M.logout()[0])
