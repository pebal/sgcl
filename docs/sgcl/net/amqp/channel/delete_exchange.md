[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::delete_exchange, async_delete_exchange

```cpp
expected<void, io::error> delete_exchange(const string& name, bool if_unused = false) const;                         // (1)
async::task<expected<void, io::error>> async_delete_exchange(string name, bool if_unused = false) const noexcept;    // (2)
```

exchange.delete: the exchange and its bindings removed; with `if_unused`, only when no queue is bound to it.

`delete_exchange` waits on the calling thread; a task awaits `async_delete_exchange`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the exchange |
| `if_unused` | refused (406) while bindings remain |


## Return value

Nothing; `errc::not_found`, `errc::precondition_failed`; the channel closes with them.

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
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_exchange("old", {.type = "fanout"}).value();
    println("{}", bool(ch.delete_exchange("old")));
}
```

Output:

```text
true
```

## See also

- [declare_exchange](declare_exchange.md)
- [channel](README.md)
