[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [broker](README.md)

# sgcl::net::mqtt::broker::close

```cpp
void close() const;
```

At once: every listener and connection closed; `serve` returns `net::errc::server_closed`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections.

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
    b.close();
    serving.wait();
    println("{}", c.receive().has_value());
}
```

Output:

```text
false
```

## See also

- [shutdown](shutdown.md)
- [broker](README.md)
