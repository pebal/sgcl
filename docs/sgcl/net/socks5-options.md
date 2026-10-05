[sgcl](../README.md) › [net](README.md) › [socks5](socks5/README.md) › options

# sgcl::net::socks5::options

```cpp
#include "sgcl/net/socks5.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct socks5 {
        struct options {
            string username;
            string password;
            duration timeout = 30 * second;
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::socks5::options` is how a connection through a SOCKS5 proxy is asked for: the credentials of RFC 1929,
the longest the whole of it may take, and a stop. A plain struct, its fields set by name; the forms of
[connect](socks5/connect.md) and [client](socks5/client.md) without it take its defaults: no credentials, 30 s, no
stop.

## Member objects

| Member | Description |
|---|---|
| `username` | offered with username/password authentication when not empty, 1 to 255 bytes; empty by default: no authentication offered but none |
| `password` | its password, 0 to 255 bytes; a password without a username is refused (`EINVAL`) |
| `timeout` | the dial of the proxy and the handshake together, `ETIMEDOUT` past it; zero or less: none; 30 s by default |
| `stop` | ends the dial or the handshake with `ECANCELED` when stopped; none by default |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    string target = l.local_endpoint().to_string();

    net::socks5::options o;
    o.username = "alice";
    o.password = "secret";
    o.timeout = 5s;
    auto c = net::socks5::connect("127.0.0.1:1080", target, o);
    println("{}", c.has_value());

    o.password = "wrong";
    auto refused = net::socks5::connect("127.0.0.1:1080", target, o);
    println("{}", refused.error().code() == net::errc::proxy_auth_required);

    async::stop_source source;
    source.request_stop();
    o.stop = source.token();
    auto stopped = net::socks5::connect("127.0.0.1:1080", target, o);
    println("{}", stopped.error().code() == std::errc::operation_canceled);
    l.close();
}
```

Output:

```text
true
true
true
```

## See also

- [connect, async_connect](socks5/connect.md), [client, async_client](socks5/client.md): what take it
- [stop_source](../async/stop_source/README.md): what makes a stop token
- [socks5](socks5/README.md)
