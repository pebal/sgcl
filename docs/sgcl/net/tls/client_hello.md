[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::client_hello

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    struct client_hello {
        string server_name;
        vector<string> alpn;
    };

    using identity_function =
        function<async::task<expected<tls::identity, io::error>>(const client_hello&)>;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::client_hello` is what a client's first ClientHello asks a server for, as the server reads it before it
chooses an identity: the name the client connects to (SNI, RFC 6066 §3) and the application protocols it offers (ALPN,
RFC 7301). A server whose [config](config.md) has an `identity_for` gives the function one per connection and serves
the [identity](identity/README.md) it returns: certificates chosen, loaded or obtained per name, as Go's
`GetCertificate` is given a `ClientHelloInfo`. An identity for the name `acme-tls/1` alone is asked for is the
challenge certificate of ACME's tls-alpn-01 (RFC 8737), which [acme::manager](../acme/manager/README.md) serves this way.

## Member objects

| Member | Description |
|---|---|
| `server_name` | the name the client sent, as it sent it; empty when it sent none (a client of an IP address sends none) |
| `alpn` | the protocols the client offers, in its order; empty when it offers none |

## Example

A server with an identity of its own for each of two names, chosen per hello:

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

// A self-signed identity for a name, made here
net::tls::identity identity_of(const string& name) {
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template t;
    t.dns_names = {name};
    auto cert = crypto::x509::create_certificate(t, key);
    auto der = vector<byte>(cert.raw().begin(), cert.raw().end());
    string pem = encoding::pem("CERTIFICATE", der).to_string();
    return net::tls::identity(pem, key.to_pem());
}

int main() {
    net::tls::identity a = identity_of("a.example");
    net::tls::identity b = identity_of("b.example");
    net::tls::config server_cfg;
    server_cfg.identity_for = [a, b](const net::tls::client_hello& hello)
        -> async::task<expected<net::tls::identity, io::error>> {
        println("asked for {}", hello.server_name);
        co_return hello.server_name == "a.example" ? a : b;
    };
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "127.0.0.1:" + to_string(incoming.local_endpoint().port());
    auto accepting = async::spawn([](net::listener l) -> async::task<> {
        for (int i : range(2)) {
            auto c = co_await l.async_accept();
            co_await c->async_close();
        }
    }(incoming));

    for (const char* name : {"a.example", "b.example"}) {
        net::tls::config cfg;
        cfg.server_name = name;
        cfg.insecure_skip_verify = true;   // self-signed: the names alone are shown
        auto c = net::tls::connect(address, cfg);
        println("served {}", net::tls::state_of(*c)->peer_certificates[0].dns_names()[0]);
        c->close();
    }
    accepting.wait();
    incoming.close();
}
```

Output:

```text
asked for a.example
served a.example
asked for b.example
served b.example
```

## See also

- [config](config.md): `identity_for`, beside `identities`
- [identity](identity/README.md): what the function returns
- [acme::manager](../acme/manager/README.md): certificates obtained per name at the first handshake
- [net::tls](README.md)
