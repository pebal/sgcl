[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md)

# sgcl::net::tls::ticket_keys

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    class ticket_keys;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::ticket_keys` is what a server seals its session tickets under (RFC 8446 §4.6.1): the keys of Go's
`tls.Config` that `SetSessionTicketKeys` sets and that Go otherwise makes and rotates by itself. A server's
[config](../config.md) holds one in `ticket_keys`, made at random with the config, so that a listener and every
connection of it, and every copy of the config, issue tickets the others resume; a server that wants tickets to
survive a change of its config copies the keys over, and two configs made apart resume each other's tickets no
more than two servers do.

A ticket is the session itself, sealed: nothing of it is kept by the server. It holds the key of the session, its
cipher suite, the time it was issued with its lifetime, and the client's certificate chain when the handshake had
one, sealed with AES-256-GCM under the current key, whose random name goes in front so that the key that opens it
is found. Two keys are kept, the current one, which seals, and the previous one, which only opens; the current one
becomes the previous when it is older than the config's `ticket_lifetime`, at the next ticket it would seal, so a
ticket resumes for its lifetime, and no key opens tickets for more than two lifetimes. [rotate](rotate.md) makes
that change at once.

It is a handle of one word whose state is made in the constructor; its keys lie in an unmanaged block, zeroed when
a key is replaced and when the state is collected, and the first is drawn by the first ticket sealed, so a client's
config, which seals none, costs no key. It is safe from many threads at once.

## Rules

- A moved-from handle is the same keys, as a moved-from [tracked_ptr](../../../core/tracked_ptr/README.md) still
  points.
- The keys live in the process: a server started again issues tickets under new ones, and the tickets of the one
  before are full handshakes.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ticket_keys.md) | keys made at random; a copy |
| [operator=](operator_assign.md) | makes this handle one of another's keys |

#### Modifiers

| Function | Description |
|---|---|
| [rotate](rotate.md) | a new current key, the current one kept as the previous |

## Example

A listener and a server of a copy of its config share the keys: a session of one resumes on the other.

```cpp
#include "sgcl/async.h"
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
    net::tls::config again = server_cfg;
    net::listener transport = net::tcp::listen("127.0.0.1:0");
    string address = "localhost:" + to_string(transport.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.session_cache = net::tls::session_cache();
    for (const net::tls::config* server : {&server_cfg, &again}) {
        auto pending = async::spawn(net::tls::async_connect(address, cfg));
        net::connection served = net::tls::server(transport.accept().value(), *server).value();
        auto c = pending.wait();
        served.write("hello\n");
        c->read_line();
        println("{}", net::tls::state_of(served)->resumed);
        c->close();
        served.close();
    }
    transport.close();
}
```

Output:

```text
false
true
```

## See also

- [config](../config.md): `ticket_keys`, `session_tickets`, `ticket_lifetime`
- [session_cache](../session_cache/README.md): where a client keeps the tickets
- [net::tls](../README.md)
