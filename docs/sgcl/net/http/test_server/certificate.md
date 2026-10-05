[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::certificate

```cpp
optional<crypto::x509::certificate> certificate() const;
```

Returns the certificate of the test CA that signed the server's (TLS only), what [client](client.md) trusts: a client
made by hand trusts the server by a [certificate_pool](../../../crypto/x509-certificate_pool/README.md) of it. The CA and the
leaf are made when the server starts, with P-256 keys of their own, valid from an hour before for a year; the leaf
names `localhost`, `127.0.0.1` and `::1`.

## Parameters

None.

## Return value

The CA's certificate; `nullopt` without TLS.

## Complexity

Constant.

## Exceptions

`invalid_argument` for a moved-from server.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request, net::http::response_writer w) {
        w.write("secure\n");
    }, {.tls = true});
    auto ca = ts.certificate();
    println("{} | CA: {}", ca->subject().to_string(), ca->is_ca());
    net::http::client mine;  // trusting the CA by hand
    mine.proxy = net::http::proxy();
    mine.tls.roots = crypto::x509::certificate_pool();
    mine.tls.roots->add(*ca);
    print("{}", mine.get(ts.url())->text().value());
}
```

Output:

```text
CN=sgcl test CA | CA: true
secure
```

## See also

- [client](client.md): a client that trusts it already
- [sgcl::net::http::test_server](README.md)
