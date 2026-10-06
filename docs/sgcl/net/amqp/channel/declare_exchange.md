[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::declare_exchange, async_declare_exchange

```cpp
expected<void, io::error> declare_exchange(const string& name, const exchange_options& o = {}) const;                  // (1)
async::task<expected<void, io::error>> async_declare_exchange(string name, exchange_options o = {}) const noexcept;    // (2)
```

exchange.declare: the exchange made with the [options](../exchange_options.md), or found as it is (declaring one
that exists with the same type does nothing). With `passive`, only checked that it exists.

`declare_exchange` waits on the calling thread; a task awaits `async_declare_exchange`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the exchange; "amq." is the broker's prefix |
| `o` | the type, passive, durable, auto-delete, internal, the arguments |


## Return value

Nothing; `errc::precondition_failed` for an exchange of another type, `errc::not_found` for a passive one that is not there, `errc::access_refused` for a name of the broker's prefix. The channel closes with any of them.

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
    ch.declare_exchange("events", {.type = "topic", .durable = true}).value();
    auto again = ch.declare_exchange("events", {.type = "direct"});
    println("{}", again.error().code() == net::amqp::errc::precondition_failed);
}
```

Output:

```text
true
```

## See also

- [exchange_options](../exchange_options.md)
- [bind_queue](bind_queue.md)
- [channel](README.md)
