[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a session: `false` for a client made by the default constructor.

## Parameters

None.

## Return value

`true` for a session.

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
    println("{} {}", bool(none), bool(c));
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
