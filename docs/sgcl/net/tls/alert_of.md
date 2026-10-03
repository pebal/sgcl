[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::alert_of

```cpp
#include "sgcl/net/tls/error.h"   // or "sgcl/net/tls.h"

namespace sgcl::net::tls {
    optional<alert> alert_of(const io::error& e) noexcept;
}
```

Returns the alert a failed connection ended with, this side's or the peer's, a server's before its hello included:
what Go's `tls.AlertError` carries, one `io::error` for all of them here. With [is_remote](is_remote.md) and
[certificate_reason](certificate_reason.md) it tells a server that refused from a certificate this side did not
take.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |

## Return value

The [alert](alert.md), or `nullopt` for an error of another category and for a chain that did not verify, whose
alert follows from its reason ([certificate_reason](certificate_reason.md)).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    server_cfg.alpn = {"echo/1"};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.alpn = {"h2"};
    auto refused = net::tls::connect(address, cfg);
    println("{}", net::tls::alert_of(refused.error()) == net::tls::alert::no_application_protocol);

    auto untrusted = net::tls::connect(address);  // the system's roots
    println("{}", net::tls::alert_of(untrusted.error()).has_value());
    incoming.close();
}
```

Output:

```text
true
false
```

## See also

- [is_remote](is_remote.md): which side sent it
- [certificate_reason](certificate_reason.md): why a chain was not trusted
- [alert](alert.md), [category](category.md)
- [net::tls](README.md)
