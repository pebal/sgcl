[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::client::close, async_close

```cpp
expected<void, io::error> close() const;                                // (1)
async::task<expected<void, io::error>> async_close() const noexcept;    // (2)
```

connection.close and its close-ok: every channel ended, its unacknowledged deliveries back to their queues, the
connection closed.

`close` waits on the calling thread; a task awaits `async_close`.

## Parameters

None.

## Return value

Nothing; the error that ended the connection before.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    println("{}", bool(c.close()));
    println("{}", bool(c.open_channel()));
}
```

Output:

```text
true
false
```

## See also

- [connect](connect.md)
- [client](README.md)
