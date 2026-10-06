[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::framing

```cpp
#include "sgcl/net/jsonrpc/peer.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    enum class framing : uint8_t { content_length, line };
}
```

How a [peer](peer/README.md) over a byte stream cuts its messages ([options](peer-options.md)).

| Value | Description |
|---|---|
| `content_length` | LSP's base protocol: a head of `Content-Length: N` (other fields passed over) and an empty line before each message |
| `line` | one JSON a line, LF ending it (a CR before it dropped), as a JSON Lines stream |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::jsonrpc::methods m;
    m.add("add", [](const encoding::json& p) -> expected<encoding::json, net::jsonrpc::error> {
        return encoding::json(p[0].as_int(0) + p[1].as_int(0));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto accepted = async::spawn(l.async_accept());
    net::connection near = net::tcp::connect(l.local_endpoint().to_string()).value();
    net::jsonrpc::peer::options o;
    o.framing = net::jsonrpc::framing::line;
    o.methods = m;
    net::jsonrpc::peer server = net::jsonrpc::peer::connect(accepted.wait().value(), o);
    net::jsonrpc::peer client = net::jsonrpc::peer::connect(near, {.framing = net::jsonrpc::framing::line});
    println("{}", client.call("add", encoding::json::array({3, 4})).value().to_string());
}
```

Output:

```text
7
```

## See also

- [options](peer-options.md)
