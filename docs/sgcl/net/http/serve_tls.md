[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve_tls

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

template<class H>
expected<void, io::error> serve_tls(const string& address, const string& certificate_file,
                                    const string& key_file, H handler);
```

Serves one handler for every path over https, Go's `http.ListenAndServeTLS(address, certFile, keyFile, handler)`: the
certificate chain and its private key are read from their PEM files, the key with
[crypto::read_secret](../../crypto/secret/README.md) so that it never passes through managed memory, made into a
[tls::identity](../tls/identity/README.md), and a [server](server/README.md) of the one route `"/"` serves them by
[serve_tls](server/serve_tls.md), with `h2` and `http/1.1` by ALPN.

It blocks the calling thread, a thread of the program's (main's). There is no handle to the server: it runs until
the program ends. A server that is stopped, or has routes, is a [server](server/README.md) of the program's.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on, `"host:port"`; no host is every address |
| `certificate_file` | the PEM file of the certificate chain, the leaf first |
| `key_file` | the PEM file of the leaf's private key |
| `handler` | a function of `(request, response_writer)` that returns `void` or `async::task<>`; another return type does not compile |

## Return value

Never a value. A file that cannot be read, or a key that is not the certificate's, is the error, before anything
listens; then the error of the listen, or of an accept.

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

## See also

- [server::serve_tls](server/serve_tls.md): a server of the program's over TLS
- [serve](serve.md): the files of a directory in one call
- [tls::identity](../tls/identity/README.md): the certificate and the key
