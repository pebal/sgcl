[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [broker](README.md)

# sgcl::net::mqtt::broker::shutdown, async_shutdown

```cpp
void shutdown() const;
async::task<> async_shutdown() const noexcept;
```

Gracefully: the listeners closed, every connection waiting for a packet told DISCONNECT with 0x8B (server shutting down, MQTT 5) and closed, the others after the packet they are in; returns when all have ended. The sessions' wills go out as for any end without DISCONNECT.

`shutdown` waits on the calling thread; a task awaits `async_shutdown`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections.

## Exceptions

- `shutdown`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_shutdown`: none.

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
    b.shutdown();
    auto r = c.receive();
    println("{:#x}", *net::mqtt::reason_of(r.error()));
    serving.wait();
}
```

Output:

```text
0x8b
```

## See also

- [close](close.md)
- [broker](README.md)
