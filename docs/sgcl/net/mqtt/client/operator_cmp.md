[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::operator==

```cpp
friend bool operator==(const client& a, const client& b) noexcept;
```

Whether the two handles are the same session: one a copy of the other.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the clients |

## Return value

`true` for the same session.

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
    net::mqtt::client copy = c;
    println("{}", copy == c);
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [connect](connect.md)
- [client](README.md)
