[sgcl](../../README.md) › [net](../README.md) › [jsonrpc](README.md)

# sgcl::net::jsonrpc::error_of

```cpp
optional<error> error_of(const io::error& e);
```

The [error](error.md) object a call's error carries: its code, message and data; none for an error of another category, or the client's own `errc::malformed`.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |


## Return value

The error object, or none.

## Complexity

Constant.

## Exceptions

None.

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
    m.add("busy", [](const encoding::json&) -> expected<encoding::json, net::jsonrpc::error> {
        return unexpected(net::jsonrpc::error(-32001, "busy", encoding::json(3)));
    });
    auto e = net::jsonrpc::error_of(client.call("busy").error());
    println("{} {} {}", e->code, e->message, e->data->to_string());
    println("{}", bool(net::jsonrpc::error_of(io::error(io::errc::closed, "x", ""))));
}
```

Output:

```text
-32001 busy 3
false
```

## See also

- [error](error.md)
