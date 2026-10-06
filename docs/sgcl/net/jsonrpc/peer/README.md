[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md)

# sgcl::net::jsonrpc::peer

```cpp
#include "sgcl/net/jsonrpc/peer.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    class peer;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::peer` is a connection of JSON-RPC 2.0 over a byte stream or a WebSocket, both ways at once, as
LSP's client and server are: this side's [calls](call.md) go out and wait for their responses, the other side's
requests go to the [options](../peer-options.md)' [methods](../methods/README.md). A connection of
`net::connection` (TCP, a unix socket, a pipe) is framed by LSP's `Content-Length` or by lines; a WebSocket carries
a message of text each.

## Rules

- A handle of one word: a copy is the same connection; calls may come from any task at once.
- A task of the module reads the connection: responses go to their calls, requests to the methods (a method that
  answers at once on that task, one that waits in a task of its own).
- After [close](close.md), the other side's close or a failure of the connection, every call fails with the error
  that ended it; [wait](wait.md) says when that happens.

## Member types

| Type | Definition |
|---|---|
| [options](../peer-options.md) | the framing, the methods served, the cancel method, the size limit |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](peer.md) | no connection, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another connection |
| [connect](connect.md) | JSON-RPC over a connection or a WebSocket |
| [call, async_call](call.md) | a call and its result |
| [notify, async_notify](notify.md) | a notification |
| [batch, async_batch](batch.md) | several in one message |
| [wait, async_wait](wait.md) | until the connection ends |
| [close](close.md) | the connection closed |
| [operator bool](operator_bool.md) | whether the handle holds a connection |
| [operator==](operator_cmp.md) | whether two handles are the same connection |

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
    println("{}", client.call("add", encoding::json::array({2, 3})).value().to_string());
    auto missing = client.call("subtract", encoding::json::array({2, 3}));
    println("{}", missing.error().code() == net::jsonrpc::errc::method_not_found);
    client.close();
}
```

Output:

```text
5
true
```

## See also

- [methods](../methods/README.md)
- [client](../client/README.md)
