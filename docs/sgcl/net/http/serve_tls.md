[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve_tls

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

template<class H>
expected<void, io::error> serve_tls(const string& address, const string& certificate_file,    // (1)
                                    const string& key_file, H handler);
template<class H>
expected<void, io::error> serve_tls(const string& address, const net::tls::config& c,         // (2)
                                    H handler);
```

Serves one handler for every path over https, Go's `http.ListenAndServeTLS(address, certFile, keyFile, handler)`: a
[server](server/README.md) of the one route `"/"` serves it by [serve_tls](server/serve_tls.md), with `h2` and
`http/1.1` by ALPN.

1. The certificate chain and its private key are read from their PEM files, the key with
   [crypto::read_secret](../../crypto/secret/README.md) so that it never passes through managed memory, and made into
   a [tls::identity](../tls/identity/README.md).
2. The TLS config is the program's: an identity of its own, or an `identity_for` that chooses one per hello, as
   [acme::manager](../acme/manager/README.md)'s [tls_config](../acme/manager/tls_config.md) does, the server's
   certificates obtained and renewed by themselves. Its ALPN list is completed as [server::serve_tls](server/serve_tls.md)
   completes it.

It blocks the calling thread, a thread of the program's (main's). There is no handle to the server: it runs until
the program ends. A server that is stopped, or has routes, is a [server](server/README.md) of the program's.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on, `"host:port"`; no host is every address |
| `certificate_file` | the PEM file of the certificate chain, the leaf first |
| `key_file` | the PEM file of the leaf's private key |
| `c` | the TLS config of the server |
| `handler` | a function of `(request, response_writer)` that returns `void` or `async::task<>`; another return type does not compile |

## Return value

Never a value. A file that cannot be read, a key that is not the certificate's, or a config a server cannot take (no
identity and no `identity_for`) is the error, before anything listens; then the error of the listen, or of an accept.

## Complexity

A task for each connection and its handshake, for as long as it lives.

## Exceptions

What the move of `handler` throws; `std::system_error` when the wait starts the scheduler and a worker's thread
cannot be started.

## Example

The test certificate of the tree (run from its root):

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto served = net::http::serve_tls(":8443", "tests/net/tls_testdata/ecdsa.pem",
                                       "tests/net/tls_testdata/ecdsa.key",
                                       [](net::http::request req, net::http::response_writer w) {
                                           w.write("hello over " + req.proto() + "\n");
                                       });
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl -s --cacert tests/net/tls_testdata/ca.pem https://localhost:8443/hello
hello over HTTP/2.0
$ curl -s --http1.1 --cacert tests/net/tls_testdata/ca.pem https://localhost:8443/hello
hello over HTTP/1.1
```

Certificates from an ACME CA, obtained at the first handshake for each name and renewed by themselves (the program
needs the names to resolve to the machine and port 443 open to the CA; Let's Encrypt's terms of service agreed to):

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::acme::manager certificates({"example.com", "www.example.com"},
                                    {.cache = "/var/lib/myapp/certs", .accept_terms = true});
    auto served = net::http::serve_tls(":443", certificates.tls_config(),
                                       [](net::http::request req, net::http::response_writer w) {
                                           w.write("hello over " + req.proto() + "\n");
                                       });
    println("{}", served.error().message());
}
```

## See also

- [server::serve_tls](server/serve_tls.md): a server of the program's over TLS
- [acme::manager](../acme/manager/README.md): certificates obtained and renewed by themselves
- [serve](serve.md): the files of a directory in one call
- [tls::identity](../tls/identity/README.md): the certificate and the key
