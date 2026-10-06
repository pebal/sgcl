[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::drain, async_drain

```cpp
expected<void, io::error> drain() const;                                // (1)
async::task<expected<void, io::error>> async_drain() const noexcept;    // (2)
```

Every subscription unsubscribed, a flush so that the server sent everything it had for them, then the connection
closed: the messages received until then stay to be read from their subscriptions.

`drain` waits on the calling thread; a task awaits `async_drain`.

## Parameters

None.

## Return value

Nothing; the connection's end before.

## Complexity

A round trip.

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
    net::nats::subscription sub = nc.subscribe("last").value();
    nc.publish("last", "words").value();
    nc.drain().value();
    println("{}", sub.receive().value().data);
    println("{}", bool(sub.receive()));
}
```

Output:

```text
words
false
```

## See also

- [close](close.md)
- [client](README.md)
