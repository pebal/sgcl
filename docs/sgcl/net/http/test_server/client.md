[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::client

```cpp
http::client client() const;
```

Returns a [client](../client/README.md) made for the server, Go's `ts.Client()`: no proxy (not the environment's),
TLS that trusts the server's test CA, HTTP/2 when the server speaks it (h2 offered by ALPN, or h2c). The same client
each time: a copy shares its pool, so the requests of a test reuse its connections, and its idle connections are
closed with the server. Its settings are a copy's own, changed on the copy without reaching the next call's.

## Parameters

None.

## Return value

The client.

## Complexity

Constant.

## Exceptions

`invalid_argument` for a moved-from server.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

using namespace std::chrono_literals;

int main() {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write(r.proto() + "\n");
    }, {.tls = true, .http2 = true});
    net::http::client c = ts.client();
    c.timeout = 5s;
    print("{}", c.get(ts.url())->text().value());
    net::http::client stranger;
    stranger.proxy = net::http::proxy();
    // the system's roots know nothing of the test CA
    println("{}", stranger.get(ts.url()).has_value());
}
```

Output:

```text
HTTP/2.0
false
```

## See also

- [certificate](certificate.md): the CA the client trusts
- [client](../client/README.md)
- [sgcl::net::http::test_server](README.md)
