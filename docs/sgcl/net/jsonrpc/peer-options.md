[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md) › [peer](peer/README.md) › options

# sgcl::net::jsonrpc::peer::options

```cpp
#include "sgcl/net/jsonrpc/peer.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    class peer {
    public:
        struct options {
            jsonrpc::framing framing = jsonrpc::framing::content_length;
            jsonrpc::methods methods;
            string cancel_method = string("$/cancelRequest");
            size_t max_message = size_t(64) << 20;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::peer::options` is how a [peer](peer/README.md) speaks, the argument of [connect](peer/connect.md).

## Member objects

| Member | Description |
|---|---|
| `framing` | over a stream, how messages are cut ([framing](framing.md)); `content_length` by default, LSP's |
| `methods` | what the other side may call; an empty table by default |
| `cancel_method` | the notification sent with `{"id": ...}` when a call is cancelled, and the one taken to cancel a request; `$/cancelRequest` by default (LSP's); empty: none |
| `max_message` | a message past it ends the connection; 64 MB by default |

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
    o.methods = m;
    net::jsonrpc::peer server = net::jsonrpc::peer::connect(accepted.wait().value(), o);
    net::jsonrpc::peer client = net::jsonrpc::peer::connect(near);
    println("{}", client.call("add", encoding::json::array({4, 5})).value().to_string());
}
```

Output:

```text
9
```

## See also

- [connect](peer/connect.md)
