[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [peer](README.md)

# sgcl::net::jsonrpc::peer::notify, async_notify

```cpp
expected<void, io::error> notify(const string& method, const json& params = json()) const;                  // (1)
async::task<expected<void, io::error>> async_notify(string method, json params = json()) const noexcept;    // (2)
```

A notification: written, nothing answered.

`notify` waits on the calling thread; a task awaits `async_notify`.

## Parameters

| Parameter | Description |
|---|---|
| `method` | the notification |
| `params` | an array or an object; null: none sent |


## Return value

Nothing; the connection's end.

## Complexity

Constant.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/jsonrpc.h"
#include "sgcl/net.h"

#include <atomic>

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
    std::atomic<int> seen = 0;
    m.add_notification("tick", [&seen](const encoding::json&) { ++seen; });
    client.notify("tick").value();
    client.call("add", encoding::json::array({0, 0})).value();  // answered after the notification was read
    println("{}", seen.load());
}
```

Output:

```text
1
```

## See also

- [call](call.md)
- [add_notification](../methods/add_notification.md)
- [peer](README.md)
