[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::client_id

```cpp
string client_id() const noexcept;
```

The client id: the options' one, the broker's when it assigned one to a client that gave none (MQTT 5), or the one the client made (`sgcl-` and 16 hex digits) for 3.1.1.

## Parameters

None.

## Return value

The id.

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
    net::mqtt::client::options o;
    o.client_id = "kitchen";
    net::mqtt::client c = net::mqtt::client::connect(url, o).value();
    println("{}", c.client_id());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
kitchen
```

## See also

- [connect](connect.md)
- [client](README.md)
