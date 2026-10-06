[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [methods](README.md)

# sgcl::net::jsonrpc::methods::add_task

```cpp
methods& add_task(const string& name,
                  function<async::task<expected<json, error>>(json params, async::stop_token stop)> f);
```

A method that waits: a task of the params and the call's stop token, which the caller's cancellation stops (a
peer's `$/cancelRequest`, an HTTP request's end). It runs as a task of its own. A method of the name there already
is replaced.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the method's name |
| `f` | the handler: a task of the params and the stop token, the result or an error object |


## Return value

The table.

## Complexity

Constant.

## Exceptions

What the copy of `f` throws.

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
    m.add_task("slow_add", [](encoding::json p, async::stop_token) -> async::task<expected<encoding::json, net::jsonrpc::error>> {
        co_await async::sleep(std::chrono::milliseconds(10));
        co_return encoding::json(p[0].as_int(0) + p[1].as_int(0));
    });
    println("{}", client.call("slow_add", encoding::json::array({1, 2})).value().to_string());
}
```

Output:

```text
3
```

## See also

- [add](add.md)
- [call](../peer/call.md)
- [methods](README.md)
