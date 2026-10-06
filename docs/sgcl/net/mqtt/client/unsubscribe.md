[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::unsubscribe, async_unsubscribe

```cpp
expected<void, io::error> unsubscribe(const string& filter) const;                                  // (1)
async::task<expected<void, io::error>> async_unsubscribe(const string& filter) const noexcept;      // (2)
expected<void, io::error> unsubscribe(const vector<string>& filters) const;                         // (3)
async::task<expected<void, io::error>> async_unsubscribe(vector<string> filters) const noexcept;    // (4)
```

UNSUBSCRIBE (MQTT 5 §3.10): the subscriptions of the filters ended. A filter of no subscription is no error (0x11, no subscription existed).

## Parameters

| Parameter | Description |
|---|---|
| `filter` | the filter, as it was subscribed |
| `filters` | the filters |

## Return value

Nothing; `errc::topic_invalid` for a filter that is not one; `errc::subscribe_refused` for a refusal.

## Complexity

One round trip.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

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
    c.subscribe("news");
    c.unsubscribe("news");
    c.publish("news", "unheard");
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

- [subscribe](subscribe.md)
- [client](README.md)
