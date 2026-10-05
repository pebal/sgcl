[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [session_cache](README.md)

# sgcl::net::tls::session_cache::clear

```cpp
void clear() const noexcept;
```

Drops every session of the cache, each one's key zeroed at once: the next connection to any server is a full
handshake. A program that has learned a server's key is compromised, or that leaves an identity behind (a user who
logs out), clears its cache. The cache stays and keeps the tickets that come after.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of sessions.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
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
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.session_cache = net::tls::session_cache();
    for (int i : range(2)) {
        auto c = net::tls::connect(address, cfg);
        net::connection served = incoming.accept().value();
        served.write("hello\n");
        c->read_line();
        println("{} {}", net::tls::state_of(*c)->resumed, cfg.session_cache->size());
        cfg.session_cache->clear();
        c->close();
        served.close();
    }
    incoming.close();
}
```

Output:

```text
false 1
false 1
```

## See also

- [size](size.md): the sessions held
- [sgcl::net::tls::session_cache](README.md)
