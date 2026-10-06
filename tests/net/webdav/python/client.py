# A WebDAV client written with Python's standard library alone
# (http.client, xml.etree): the requests of RFC 4918 by hand and the
# multistatus read by an XML parser of its own, against the module's server.
# Prints one line per step.
import http.client
import sys
import xml.etree.ElementTree as ET

port = int(sys.argv[1])
D = "{DAV:}"


def req(method, path, body=None, headers=None):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    data = r.read()
    c.close()
    return r.status, dict(r.getheaders()), data


status, _, _ = req("MKCOL", "/dav/py")
print("mkcol", status)
status, _, _ = req("PUT", "/dav/py/a%20b.txt", body=b"hello from python")
print("put", status)
status, _, data = req("PROPFIND", "/dav/py/", body=b'<?xml version="1.0"?><propfind xmlns="DAV:"><allprop/></propfind>', headers={"Depth": "1"})
root = ET.fromstring(data)
hrefs = sorted(r.find(D + "href").text for r in root.findall(D + "response"))
sizes = [p.text for p in root.iter(D + "getcontentlength")]
print("propfind", status, " ".join(hrefs), " ".join(sizes))
status, _, data = req("GET", "/dav/py/a%20b.txt")
print("get", status, data.decode())
lockinfo = b'<?xml version="1.0"?><lockinfo xmlns="DAV:"><lockscope><exclusive/></lockscope><locktype><write/></locktype><owner>py</owner></lockinfo>'
status, headers, data = req("LOCK", "/dav/py/a%20b.txt", body=lockinfo, headers={"Timeout": "Second-60"})
token = headers.get("Lock-Token", "")
print("lock", status, token.startswith("<urn:uuid:"))
status, _, _ = req("PUT", "/dav/py/a%20b.txt", body=b"x")
print("put-locked", status)
status, _, _ = req("PUT", "/dav/py/a%20b.txt", body=b"changed", headers={"If": "(" + token + ")"})
print("put-token", status)
status, _, _ = req("UNLOCK", "/dav/py/a%20b.txt", headers={"Lock-Token": token})
print("unlock", status)
status, _, _ = req("MOVE", "/dav/py/a%20b.txt", headers={"Destination": "http://127.0.0.1:%d/dav/py/c.txt" % port})
print("move", status)
status, _, data = req("GET", "/dav/py/c.txt")
print("moved", status, data.decode())
status, _, _ = req("DELETE", "/dav/py")
print("delete", status)
