[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::flush, async_flush

```cpp
expected<void, io::error> flush() const;                                // (1)
async::task<expected<void, io::error>> async_flush() const noexcept;    // (2)
```

PING, and its PONG waited for: everything written before it, the buffered publications too, was read by the
server — and a subscription made before it is in place there.

`flush` waits on the calling thread; a task awaits `async_flush`.

## Parameters

None.

## Return value

Nothing; `ETIMEDOUT` past the options' timeout, the connection's end.

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
    for (int i = 0; i < 1000; ++i) {
        nc.publish("metrics", "x").value();
    }
    println("{}", bool(nc.flush()));
}
```

Output:

```text
true
```

## See also

- [publish](publish.md)
- [client](README.md)
