[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::tls

```cpp
optional<net::tls::state> tls() const noexcept;
```

Returns what the TLS handshake of the connection a received request came over settled, Go's `r.TLS`: a
[net::tls::state](../../tls/state.md) with the cipher suite, the group, the name the client sent, the ALPN protocol,
whether the session was resumed and, when the server asked for one ([client_auth](../../tls/client_auth.md)), the
client's certificate chain, the leaf first: what a handler that authenticates clients by certificate reads. A
request over plain TCP (`http://`, h2c) has none, and so has a request the program built to send.

## Parameters

None.

## Return value

The state of the connection's handshake, a copy; `nullopt` for a connection without TLS and for a client's request.

## Complexity

Linear in the length of the client's chain, which is copied.

## Exceptions

None.

## Example

A server that requires a client certificate of the test CA greets the client by its certificate's name.

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/tls.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::tls::config settings;
    settings.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    settings.client_auth = net::tls::client_auth::require;
    settings.client_roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    settings.alpn = {"http/1.1"};

    net::http::server srv;
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        net::tls::state s = req.tls().value();
        w.write("hello, " + s.peer_certificates[0].subject().common_name() + "\n");
    });
    net::listener listener = net::tls::listen("127.0.0.1:0", settings);
    auto serving = async::spawn(srv.async_serve(listener));

    net::http::client web;
    web.tls.roots = settings.client_roots;
    web.tls.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/client_ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/client_ecdsa.key"))};
    string url = "https://localhost:" + to_string(listener.local_endpoint().port()) + "/";
    print("{}", web.get(url)->text().value());
    srv.close();
}
```

Output:

```text
hello, sgcl test client ecdsa
```

## See also

- [remote_endpoint](remote_endpoint.md): the client's address
- [net::tls::state](../../tls/state.md), [net::tls::client_auth](../../tls/client_auth.md)
- [sgcl::net::http::request](README.md)
