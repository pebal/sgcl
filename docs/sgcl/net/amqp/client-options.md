[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md) › [client](client/README.md) › options

# sgcl::net::amqp::client::options

```cpp
#include "sgcl/net/amqp/client.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    class client {
    public:
        struct options {
            net::tls::config tls;
            duration heartbeat = std::chrono::seconds(60);
            uint32_t frame_max = 131072;
            uint16_t channel_max = 2047;
            duration timeout = std::chrono::seconds(30);
            string name;
            size_t queue = 1000;
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::client::options` is how a [client](client/README.md) talks to its broker, the argument of
[connect](client/connect.md).

## Member objects

| Member | Description |
|---|---|
| `tls` | the [tls::config](../tls/config.md) of `amqps://`: the roots, a client certificate; the server's name the URL's host when none is set |
| `heartbeat` | the interval asked for; the lower of both sides' is taken, a broker silent for two of them ends the connection; 60 s by default; zero: none |
| `frame_max` | the largest frame asked for (4096 at least); a larger body goes in several frames; 128 KB by default |
| `channel_max` | the channels at most; 2047 by default |
| `timeout` | the dial, TLS and the handshake together, then each method's wait; 30 s by default; zero: none |
| `name` | the connection's name the broker shows (RabbitMQ's management) |
| `queue` | the deliveries a consumer keeps until they are received; past it the connection waits; 1000 by default |
| `stop` | ends the dial with `ECANCELED` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client::options o;
    o.heartbeat = std::chrono::seconds(10);
    o.name = "orders-service";
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/", o).value();
    println("{}", bool(c));
}
```

Output:

```text
true
```

## See also

- [connect](client/connect.md)
