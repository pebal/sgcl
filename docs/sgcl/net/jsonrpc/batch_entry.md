[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::batch_entry

```cpp
#include "sgcl/net/jsonrpc/peer.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    struct batch_entry {
        string method;
        json params;
        bool notification = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::batch_entry` is one call or notification of a [batch](peer/batch.md).

## Member objects

| Member | Description |
|---|---|
| `method` | the method |
| `params` | an array or an object; null by default: none sent |
| `notification` | a notification: no id, no answer; false by default |

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
    auto r = client.batch({{"add", encoding::json::array({1, 2})}, {"log", encoding::json(), true}}).value();
    println("{} {}", r.size(), r[0]->to_string());
}
```

Output:

```text
1 3
```

## See also

- [batch](peer/batch.md)
