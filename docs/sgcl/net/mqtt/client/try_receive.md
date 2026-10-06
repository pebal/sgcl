[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::try_receive

```cpp
optional<message> try_receive() const;
```

The next message when one is there, at once; none otherwise.

## Parameters

None.

## Return value

The message, or none.

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
    println("{}", c.try_receive().has_value());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
false
```

## See also

- [receive](receive.md)
- [client](README.md)
