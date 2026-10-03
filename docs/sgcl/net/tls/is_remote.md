[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::is_remote

```cpp
#include "sgcl/net/tls/error.h"   // or "sgcl/net/tls.h"

namespace sgcl::net::tls {
    bool is_remote(const io::error& e) noexcept;
}
```

Checks whether a failed connection ended with an alert the peer sent, rather than one this side sent: Go's
`remote error: tls: …`, whose text the error has. An alert a server sent before its hello is the peer's too.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |

## Return value

`true` for an alert the peer sent; `false` for one this side sent, a chain that did not verify and an error of
another category.

## Complexity

Constant.

## Exceptions

None.

## Example

Both sides of a handshake the server refuses:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::connection transport, net::tls::config cfg) {
    auto c = co_await net::tls::async_server(transport, cfg);
    if (!c) {
        println("server: {}", net::tls::is_remote(c.error()));
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    server_cfg.alpn = {"echo/1"};
    auto [near, far] = net::connection::in_memory();
    auto serving = async::spawn(serve(far, server_cfg));
    auto c = net::tls::client(near, {.alpn = {"h2"}, .insecure_skip_verify = true});
    serving.wait();
    println("client: {}", net::tls::is_remote(c.error()));
}
```

Output:

```text
server: false
client: true
```

## See also

- [alert_of](alert_of.md): the alert
- [certificate_reason](certificate_reason.md): why a chain was not trusted
- [category](category.md)
- [net::tls](README.md)
