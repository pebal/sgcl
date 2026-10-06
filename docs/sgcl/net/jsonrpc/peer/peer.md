[sgcl](../../../README.md) › [net](../../README.md) › [jsonrpc](../README.md) › [peer](README.md)

# sgcl::net::jsonrpc::peer::peer

```cpp
peer() noexcept;                     // (1)
peer(const peer& other) noexcept;    // (2)
```

1. No connection: `operator bool` is false; an operation on it is a contract violation.
2. The same connection as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied |


## Return value

None.

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
    net::jsonrpc::peer none;
    net::jsonrpc::peer same = client;
    println("{} {}", bool(none), same == client);
}
```

Output:

```text
false true
```

## See also

- [connect](connect.md)
- [peer](README.md)
