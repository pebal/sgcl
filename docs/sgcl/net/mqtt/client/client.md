[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::client

```cpp
client() noexcept = default;             // (1)
client(const client& other) noexcept;    // (2), implicitly declared
```

1. No session: a call on it is a contract violation; [connect](connect.md) makes one.
2. The same session: the handle copied.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the client to share |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(b.async_serve(l));
    string url = string::concat("mqtt://", l.local_endpoint().to_string());
    net::mqtt::client none;
    net::mqtt::client c = net::mqtt::client::connect(url).value();
    net::mqtt::client copy = c;
    println("{} {}", bool(none), copy == c);
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
false true
```

## See also

- [connect](connect.md)
- [client](README.md)
