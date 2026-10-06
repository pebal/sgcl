[sgcl](../../README.md) › [net](../README.md) › [nats](README.md) › [client](client/README.md) › options

# sgcl::net::nats::client::options

```cpp
#include "sgcl/net/nats/client.h"   // or "sgcl/net/nats.h"

namespace sgcl::net::nats {
    class client {
    public:
        struct options {
            string user;
            string password;
            string token;
            string nkey_seed;
            string jwt;
            string name;
            net::tls::config tls;
            duration ping_interval = std::chrono::minutes(2);
            int max_pings_out = 2;
            duration timeout = std::chrono::seconds(30);
            size_t queue = 65536;
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::nats::client::options` is how a [client](client/README.md) talks to its server, the argument of
[connect](client/connect.md).

## Member objects

| Member | Description |
|---|---|
| `user`, `password` | the login; empty: the URL's |
| `token` | an authorization token; empty: the URL's user when it has no password |
| `nkey_seed` | a user's nkey seed ("SU..."): the server's nonce signed with its Ed25519 key, the public key sent beside |
| `jwt` | a user JWT sent beside the nkey (decentralized authentication) |
| `name` | the connection's name the server shows |
| `tls` | the [tls::config](../tls/config.md) of `tls://` and of a server that requires TLS; the server's name the URL's host when none is set |
| `ping_interval` | how often a PING goes out; 2 minutes by default; zero: none |
| `max_pings_out` | the PINGs unanswered after which the connection is stale and ends; 2 by default |
| `timeout` | the dial, TLS and CONNECT together, then flush's wait; 30 s by default; zero: none |
| `queue` | the messages a subscription keeps until they are received; past it they are dropped; 65536 by default |
| `stop` | ends the dial with `ECANCELED` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client::options o;
    o.name = "billing";
    o.ping_interval = std::chrono::seconds(30);
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222", o).value();
    println("{}", bool(nc));
}
```

Output:

```text
true
```

## See also

- [connect](client/connect.md)
