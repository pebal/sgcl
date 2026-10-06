[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::properties

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct properties {
        string content_type;
        string content_encoding;
        table headers;
        amqp::delivery_mode delivery_mode = amqp::delivery_mode::none;
        uint8_t priority = 0;
        string correlation_id;
        string reply_to;
        string expiration;
        string message_id;
        optional<time::datetime> timestamp;
        string type;
        string user_id;
        string app_id;

        friend bool operator==(const properties&, const properties&) noexcept = default;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::properties` is what a message carries beside its body (0-9-1 §4.2.6, the basic class's properties). Empty, zero and none are not sent.

## Member objects

| Member | Description |
|---|---|
| `content_type` | the body's MIME type: "application/json" |
| `content_encoding` | its encoding: "gzip" |
| `headers` | the application's own [table](field/README.md); a headers exchange routes by them |
| `delivery_mode` | kept in memory or on disk too ([delivery_mode](delivery_mode.md)) |
| `priority` | 0 to 9, for a priority queue |
| `correlation_id` | of a reply: the request's id |
| `reply_to` | the queue a reply goes to |
| `expiration` | RabbitMQ's time to live, milliseconds as text |
| `message_id` | the message's id |
| `timestamp` | when it was made, in whole seconds |
| `type` | the application's name of its kind |
| `user_id` | checked against the connection's user by RabbitMQ |
| `app_id` | the application that made it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_queue("rpc").value();
    net::amqp::properties p;
    p.reply_to = "replies";
    p.correlation_id = "req-7";
    p.priority = 5;
    ch.publish("", "rpc", "ping", p).value();
    auto d = ch.get("rpc", true).value();
    println("{} {} {}", d->properties.reply_to, d->properties.correlation_id, d->properties.priority);
}
```

Output:

```text
replies req-7 5
```

## See also

- [publish](channel/publish.md)
- [delivery](delivery.md)
