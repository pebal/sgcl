[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md)

# sgcl::net::nats::message

```cpp
#include "sgcl/net/nats/types.h"   // or "sgcl/net/nats.h"

namespace sgcl::net::nats {
    struct message {
        string subject;
        string reply;
        string data;
        vector<pair<string, string>> headers;
        int status = 0;
        string description;

        message();
        message(const string& subject, const string& data);

        string header(const string& name) const;

        friend bool operator==(const message&, const message&) noexcept = default;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::nats::message` is a message of NATS: its subject, the subject a reply goes to, its data, and the headers
of NATS/1.0 with the status a server may set ("503" no responders). A plain value, what
[publish](../client/publish.md) takes and a [subscription](../subscription/README.md) gives.

## Member objects

| Member | Description |
|---|---|
| `subject` | the subject |
| `reply` | the subject a reply goes to; empty: none asked for |
| `data` | the bytes |
| `headers` | the headers in their order, a name given more than once kept so; none: PUB and MSG |
| `status` | the status of the header block; 0 for none |
| `description` | the status's text ("No Responders") |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](message.md) | an empty message, or one of a subject and data |
| [header](header.md) | the first value of a header |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription sub = nc.subscribe("audit").value();
    net::nats::message m("audit", "user deleted");
    m.headers = {{"Actor", "admin"}, {"Trace-Id", "4bf92f35"}};
    nc.publish(m).value();
    auto got = sub.receive().value();
    for (auto& [name, value] : got.headers) {
        println("{}: {}", name, value);
    }
}
```

Output:

```text
Actor: admin
Trace-Id: 4bf92f35
```

## See also

- [publish](../client/publish.md)
- [subscription](../subscription/README.md)
