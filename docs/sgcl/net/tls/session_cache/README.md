[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md)

# sgcl::net::tls::session_cache

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    class session_cache;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::session_cache` is where a client keeps the sessions its servers' tickets give it, to resume them on its
next connection (RFC 8446 §2.2): Go's `tls.ClientSessionCache` as `tls.NewLRUClientSessionCache` makes it. A
client's [config](../config.md) names one in `session_cache`; a server's ticket, which comes with its first write
after the handshake, becomes a session there, kept by the server's name, port and ALPN protocols, and the next
connection to the same server with the same protocols offers it. A resumed handshake sends no certificate and no
signature but makes a fresh key exchange (`psk_dhe_ke`); the [state](../state.md) says `resumed`, and keeps the
server's chain of the handshake that made the session.

A session is offered once, as RFC 8446 §C.4 recommends, so that two connections cannot be linked by their ticket:
the server's next ticket replaces it. A cache holds at most four sessions of a server and at most `capacity` in all,
the oldest dropped first; a session past its lifetime is dropped when its server is next looked up. An
[http::client](../../http/client/README.md) has a cache of its own by default, shared by its connections.

The same cache keeps the sessions of a server of TLS 1.2 (RFC 5246 §7.3), which come in the handshake itself: the
server's ticket (RFC 5077), which the client asks for while the config's `session_tickets` is on, or the session id
the server named. The next connection offers the ticket, or the session id when there is no ticket or tickets are
off, and a server that takes it makes the abbreviated handshake: no certificates and no key exchange, the keys made
of the session's master secret. A 1.2 session is offered once too, but kept again after the server resumed it, its
ticket replaced when the server renews it, since RFC 5077 uses a ticket until the server gives another; it lives for
the ticket's lifetime hint, seven days at most, or a day where the server names none, from the full handshake that
made it. A session the server declines is dropped, the session of the full handshake taking its place.

It is a handle of one word whose state is made in the constructor: copies, a config's among them, are the same
cache, and it is safe from many threads at once. The key of each session lies in an unmanaged block of its own,
zeroed when the session is offered, dropped or cleared, or collected.

## Rules

- A moved-from cache is the same cache, as a moved-from [tracked_ptr](../../../core/tracked_ptr/README.md) still
  points.
- Nothing of it waits: its calls take a mutex of its own for the time of a look over at most `capacity` entries.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](session_cache.md) | a cache of a capacity; a copy |
| [operator=](operator_assign.md) | makes this handle one of another's cache |

#### Capacity

| Function | Description |
|---|---|
| [capacity](capacity.md) | the sessions it holds at most |
| [size](size.md) | the sessions it holds now |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | drops every session, its key zeroed |

## Example

Two connections to one server: the first a full handshake, whose ticket comes with the server's first line, the
second resumed.

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
        println("{} {} {}", i, net::tls::state_of(*c)->resumed, cfg.session_cache->size());
        c->close();
        served.close();
    }
    incoming.close();
}
```

Output:

```text
0 false 1
1 true 1
```

## See also

- [config](../config.md): `session_cache`, the client's setting; `session_tickets`, whether a client asks a server
  of 1.2 for tickets
- [ticket_keys](../ticket_keys/README.md): what a server seals its tickets under
- [state](../state.md): `resumed`
- [net::tls](../README.md)
