[sgcl](../../README.md) › [net](../README.md) › [mqtt](README.md)

# sgcl::net::mqtt::reason_of

```cpp
optional<uint8_t> reason_of(const io::error& e) noexcept;
```

The reason code an error of the category `"mqtt"` carries (MQTT 5 §2.4): 0x87 for "not authorized", 0x8E for "session taken over"; none for an error without one or of another category.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |

## Return value

The reason code, or none.

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
    o.client_id = "same";
    net::mqtt::client first = net::mqtt::client::connect(url, o).value();
    net::mqtt::client second = net::mqtt::client::connect(url, o).value();
    auto r = first.receive();
    println("{:#x} {}", *net::mqtt::reason_of(r.error()), r.error().path());
    second.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
0x8e 0x8E Session taken over
```

## See also

- [errc](errc.md)
- [mqtt](README.md)
