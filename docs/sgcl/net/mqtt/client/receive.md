[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::receive, async_receive

```cpp
expected<message, io::error> receive() const;
async::task<expected<message, io::error>> async_receive() const noexcept;
```

The next message of the subscriptions, waited for, in the order the broker sent them; its topic is the full topic whatever alias it came by. Once the messages before it are taken, the session's end is the error: `errc::disconnected` with the broker's reason code (0x8E session taken over, 0x8B server shutting down), the connection's error, or `io::errc::closed` after [disconnect](disconnect.md).

`receive` waits on the calling thread; a task awaits `async_receive`.

## Parameters

None.

## Return value

The [message](../message/README.md).

## Complexity

Constant, and the wait.

## Exceptions

- `receive`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_receive`: none.

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
    c.subscribe("t");
    c.publish("t", "one");
    c.publish("t", "two");
    println("{}", c.receive()->text());
    println("{}", c.receive()->text());
    c.disconnect();
    println("{}", c.receive().has_value());
    b.close();
    serving.wait();
}
```

Output:

```text
one
two
false
```

## See also

- [try_receive](try_receive.md)
- [client](README.md)
