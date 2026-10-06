[sgcl](../../README.md) › [net](../README.md) › [nats](README.md)

# sgcl::net::nats::errc

```cpp
#include "sgcl/net/nats/error.h"   // or "sgcl/net/nats.h"

namespace sgcl::net::nats {
    enum class errc {
        server_error = 1,
        authorization_violation,
        permissions_violation,
        no_responders,
        max_payload,
        slow_consumer,
        invalid_subject,
        malformed
    };
}
```

The failures of NATS that neither `errno` nor [io](../../io/errc.md) names, in the category `"nats"` ([category](category.md)); an error's path is the server's text after `-ERR`.

| Value | Description |
|---|---|
| `server_error` | "NATS server error": the server's -ERR of another kind |
| `authorization_violation` | "authorization violation": the credentials refused, the authentication timed out or expired |
| `permissions_violation` | "permissions violation": a subject the user may not publish or subscribe to |
| `no_responders` | "no responders": a request whose subject no one subscribes to |
| `max_payload` | "maximum payload exceeded": a message past the server's max_payload |
| `slow_consumer` | "slow consumer": the server dropped the connection for reading too slowly |
| `invalid_subject` | "invalid subject": a subject or a queue group that may not be used |
| `malformed` | "malformed NATS protocol line": a line of the server's that breaks the protocol |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    auto r = nc.publish("orders.*", "x");
    println("{}", r.error().code() == net::nats::errc::invalid_subject);
}
```

Output:

```text
true
```

## See also

- [category](category.md)
