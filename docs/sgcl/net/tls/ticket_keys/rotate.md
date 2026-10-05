[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ticket_keys](README.md)

# sgcl::net::tls::ticket_keys::rotate

```cpp
void rotate() const noexcept;
```

Draws a new current key at once: the current one becomes the previous, which still opens the tickets sealed under
it, and the previous one is zeroed, so its tickets no longer resume (a full handshake is all they cost). The keys
rotate by themselves once the current one is older than the config's `ticket_lifetime`; `rotate` is for a server
that rotates on a schedule of its own, or that has reason to think a key known, and then calls it twice.

## Parameters

None.

## Return value

None.

## Complexity

Constant: 40 bytes of the system's random generator and an AES key schedule.

## Exceptions

None.

## Example

A session of the first connection resumes after one rotation; its successor's ticket, of the key after it, is gone
two rotations later.

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
    for (int rotations : {0, 1, 2}) {
        for (int i : range(rotations)) {
            server_cfg.ticket_keys.rotate();
        }
        auto c = net::tls::connect(address, cfg);
        net::connection served = incoming.accept().value();
        served.write("hello\n");
        c->read_line();
        println("{}", net::tls::state_of(*c)->resumed);
        c->close();
        served.close();
    }
    incoming.close();
}
```

Output:

```text
false
true
false
```

## See also

- [(constructor)](ticket_keys.md): keys of their own
- [sgcl::net::tls::ticket_keys](README.md)
