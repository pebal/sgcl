[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::close

```cpp
expected<void, io::error> close() const noexcept;
```

The connection closed without DISCONNECT: the broker publishes the will, after its delay.

## Parameters

None.

## Return value

What closing the connection gave.

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
    net::mqtt::client watcher = net::mqtt::client::connect(url).value();
    watcher.subscribe("status/#");
    net::mqtt::client::options o;
    o.will = net::mqtt::message("status/sensor-1", "offline");
    net::mqtt::client sensor = net::mqtt::client::connect(url, o).value();
    sensor.close();
    println("{}", watcher.receive()->text());
    watcher.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
offline
```

## See also

- [disconnect](disconnect.md)
- [client](README.md)
