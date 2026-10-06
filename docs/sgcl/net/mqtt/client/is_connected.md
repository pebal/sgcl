[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::is_connected

```cpp
bool is_connected() const noexcept;
```

Whether the session goes on: not disconnected, closed or ended by the broker or the connection.

## Parameters

None.

## Return value

`true` while it goes on.

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
    net::mqtt::client c = net::mqtt::client::connect(url).value();
    println("{}", c.is_connected());
    c.disconnect();
    println("{}", c.is_connected());
    b.close();
    serving.wait();
}
```

Output:

```text
true
false
```

## See also

- [connect](connect.md)
- [client](README.md)
