[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::call_options

```cpp
#include "sgcl/net/jsonrpc/peer.h"   // or "sgcl/net/jsonrpc.h"

namespace sgcl::net::jsonrpc {
    struct call_options {
        duration timeout = std::chrono::seconds(30);
        async::stop_token stop;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::jsonrpc::call_options` is a [call](peer/call.md)'s waits.

## Member objects

| Member | Description |
|---|---|
| `timeout` | the reply waited for at most this; 30 s by default; zero: no limit |
| `stop` | cancels the call: the other side is sent the cancel method, the call fails with `ECANCELED` at once |

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
    m.add_task("forever", [](encoding::json, async::stop_token stop) -> async::task<expected<encoding::json, net::jsonrpc::error>> {
        co_await async::select(stop.on_stop([] {}), async::timeout(std::chrono::seconds(1), [] {}));
        co_return unexpected(net::jsonrpc::error(net::jsonrpc::errc::request_cancelled, "cancelled"));
    });
    auto r = client.call("forever", encoding::json(), {.timeout = std::chrono::milliseconds(50)});
    println("{}", r.error().is_timeout());
    client.close();
    server.close();  // the handler's stop token stopped
}
```

Output:

```text
true
```

## See also

- [call](peer/call.md)
