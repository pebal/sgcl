[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::receive, async_receive

```cpp
expected<message, io::error> receive() const;                                // (1)
async::task<expected<message, io::error>> async_receive() const noexcept;    // (2)
```

The next message, waited for. After the subscription's end, the ones received before, then the reason.

`receive` waits on the calling thread; a task awaits `async_receive`.

## Parameters

None.

## Return value

The [message](../message/README.md); after the end, its reason (`io::errc::closed` after [unsubscribe](unsubscribe.md), `errc::permissions_violation`, the connection's end).

## Complexity

Constant.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription sub = nc.subscribe("events.>").value();
    nc.flush().value();
    nc.publish("events.ping", "1").value();
    println("{}", sub.receive().value().data);
}
```

Output:

```text
1
```

## See also

- [try_receive](try_receive.md)
- [subscription](README.md)
