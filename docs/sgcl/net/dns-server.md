[sgcl](../README.md) › [net](README.md) › [dns](dns/README.md) › server

# sgcl::net::dns::server

```cpp
#include "sgcl/net/dns.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct dns {
        struct server {
            string address;
            string name;
            vector<string> pins;

            server() noexcept = default;
            server(const char* text) noexcept;
            server(const string& text) noexcept;

            friend bool operator==(const server&, const server&) noexcept = default;
        };

        static constexpr const char* cloudflare = "1.1.1.1";
        static constexpr const char* cloudflare_tls = "tls://1.1.1.1";
        static constexpr const char* cloudflare_https = "https://cloudflare-dns.com/dns-query";
        static constexpr const char* google = "8.8.8.8";
        static constexpr const char* google_tls = "tls://8.8.8.8";
        static constexpr const char* google_https = "https://dns.google/dns-query";
        static constexpr const char* quad9 = "9.9.9.9";
        static constexpr const char* quad9_tls = "tls://9.9.9.9";
        static constexpr const char* quad9_https = "https://dns.quad9.net/dns-query";
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns::server` is a server a lookup asks, one of [options](dns-options.md)`::servers`: its address with
the transport as a scheme, and for an encrypted one the name its certificate is checked for and the keys it is
pinned to. It is made from its text wherever a server is wanted, so `o.servers = {"tls://1.1.1.1"}` is a list of
one. The transports are those of the stub resolvers of other platforms, in one field:

- `"10.0.0.53"`, `"10.0.0.53:5353"`, `"[::1]:53"`, `"udp://10.0.0.53"`: an address with an optional port (53),
  asked over UDP, and again over TCP when the answer comes truncated (RFC 7766);
- `"tcp://10.0.0.53"`: over TCP alone, as `use-vc` of resolv.conf(5);
- `"tls://1.1.1.1"`, `"tls://dns.google:853"`: DNS over TLS (RFC 7858), port 853 by default, a name dialed through
  the system's resolver; the connection is kept and shared by every lookup that asks the server, its queries in
  flight together and their answers taken in any order, each query padded to 128 bytes (RFC 8467);
- `"https://cloudflare-dns.com/dns-query"`: DNS over HTTPS (RFC 8484), the URL of the server's template, POST (GET
  with [options](dns-options.md)`::https_get`), HTTP/2 offered first by ALPN; the queries go through an
  [http::client](http/client/README.md) of the resolver's own, whose pool keeps the connections.

TLS needs `sgcl/net/tls.h` among the program's headers and HTTPS `sgcl/net/http.h`: they install the transports, so
that `sgcl/net/dns.h` itself needs neither; a server of a transport no header installed is that server's failure,
`EPROTONOSUPPORT`. The constants name the public resolvers of Cloudflare, Google and Quad9 in the three forms; their
certificates hold their addresses, so a TLS server of an address is checked as one.

## Member objects

| Member | Description |
|---|---|
| `address` | the server with its transport, as above: `net::errc::invalid_address` from the lookup when it is not one (a name without `tls://`, a port of 0, an `http://` URL) |
| `name` | TLS and HTTPS: the name the server's certificate is checked for, when it is not the address's host (`"cloudflare-dns.com"` for `"tls://104.16.248.249"`); empty by default: the host of the address, an address checked as one |
| `pins` | TLS: SPKI pins of RFC 7858 §4.2, each the base64 of the SHA-256 of a SubjectPublicKeyInfo; when there are some, the server's chain is checked against them in place of the roots, and a chain none of whose keys matches is `tls::alert::bad_certificate` (the opportunistic profile takes it anyway); a pin that is not base64 of 32 bytes is `EINVAL` from the lookup; none by default |

## Member functions

| Function | Description |
|---|---|
| `server()` | a server of no address: an empty address is `net::errc::invalid_address` |
| `server(const char*)`, `server(const string&)` | the server of the text, its name and pins empty; not explicit, so that the text stands where a server is wanted |
| `operator==` | the three fields compared |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"
#include "sgcl/net.h"

using namespace sgcl;

// A DNS over TLS server on the loopback, answering every query with "refused"
async::task<> refuse(net::listener l) {
    for (;;) {
        auto c = co_await l.async_accept();
        if (!c) {
            co_return;
        }
        vector<byte> q(514);
        if (co_await c->async_read_full(q.as_slice(0, 2))) {
            size_t len = size_t(q[0]) << 8 | size_t(q[1]);
            if (co_await c->async_read_full(q.as_slice(2, len))) {
                q[4] = byte(q[4] | byte(0x80));   // a response
                q[5] = byte(5);                    // REFUSED
                co_await c->async_write(q.as_slice(0, len + 2));
            }
        }
        c->close();
    }
}

int main() {
    auto cert = io::read_text("tests/net/tls_testdata/ecdsa.pem");
    net::tls::config tls;
    tls.identities = {net::tls::identity(cert, crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener l = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(refuse(l));

    // its key pinned: no roots needed
    auto key = crypto::x509::certificate::from_pem(cert)->raw_subject_public_key_info();
    net::dns::server pinned = "tls://" + l.local_endpoint().to_string();
    pinned.pins = {encoding::base64::standard.encode(crypto::sha256::of(key))};
    net::dns::options o;
    o.servers = {pinned};
    println("{}", net::dns::lookup_mx("example.com", o).error().message());

    o.servers[0].pins = {encoding::base64::standard.encode(crypto::sha256::of("another key"))};
    println("{}", net::dns::lookup_mx("example.com", o).error().message());
    l.close();
    serving.wait();
}
```

Output:

```text
lookup example.com: DNS server misbehaving
lookup example.com: tls: bad certificate
```

## See also

- [dns::options](dns-options.md): the list of servers and the rest of a lookup's settings
- [tls::config](tls/config.md): the TLS of a connection of the program's own
- [sgcl::net::dns](dns/README.md)
