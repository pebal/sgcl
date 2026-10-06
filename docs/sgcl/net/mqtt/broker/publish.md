[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [broker](README.md)

# sgcl::net::mqtt::broker::publish

```cpp
expected<void, io::error> publish(const message& m) const;
```

A message to the subscribers of its topic, as if a client had published it (retained when it says so): what a program that embeds the broker announces itself.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the [message](../message/README.md) |

## Return value

Nothing; `errc::topic_invalid` for a topic that is none.

## Complexity

Linear in the sessions.

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
    c.subscribe("announcements");
    b.publish(net::mqtt::message("announcements", "maintenance at noon"));
    println("{}", c.receive()->text());
    c.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
maintenance at noon
```

## See also

- [client::publish](../client/publish.md)
- [broker](README.md)
