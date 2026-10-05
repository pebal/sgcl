[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [session_cache](README.md)

# sgcl::net::tls::session_cache::size

```cpp
size_t size() const noexcept;
```

Returns the sessions the cache holds now, of every server: one for each ticket kept and not yet offered. A session
past its lifetime is counted until its server is next looked up.

## Parameters

None.

## Return value

The number of sessions, at most [capacity](capacity.md).

## Complexity

Constant.

## Exceptions

None.

## Example

A ticket comes with the server's first write: the cache has it once the client has read.

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ed25519.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ed25519.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);

    net::tls::session_cache sessions;
    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.session_cache = sessions;
    auto c = net::tls::connect("localhost:" + to_string(incoming.local_endpoint().port()), cfg);
    net::connection served = incoming.accept().value();
    println("{}", sessions.size());
    served.write("hello\n");
    c->read_line();
    println("{}", sessions.size());
    c->close();
    served.close();
    incoming.close();
}
```

Output:

```text
0
1
```

## See also

- [capacity](capacity.md): the sessions held at most
- [clear](clear.md): every session dropped
- [sgcl::net::tls::session_cache](README.md)
