[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::bind_queue, async_bind_queue

```cpp
expected<void, io::error> bind_queue(const string& queue, const string& exchange, const string& routing_key,    // (1)
                                     const table& arguments = {}) const;
async::task<expected<void, io::error>> async_bind_queue(string queue, string exchange, string routing_key,      // (2)
                                                        table arguments = {}) const noexcept;
```

queue.bind: the exchange's messages of the routing key to the queue — a direct exchange's key itself, a topic
exchange's pattern ("*" a word, "#" any words), nothing for a fanout; a headers exchange matches the arguments
(`x-match` `all` or `any`).

`bind_queue` waits on the calling thread; a task awaits `async_bind_queue`.

## Parameters

| Parameter | Description |
|---|---|
| `queue` | the queue |
| `exchange` | the exchange |
| `routing_key` | the key or pattern |
| `arguments` | a headers exchange's headers and `x-match` |


## Return value

Nothing; `errc::not_found` for a queue or an exchange that is not there, `errc::access_refused` on the default exchange.

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
    ch.declare_exchange("orders", {.type = "topic"}).value();
    ch.declare_queue("eu").value();
    ch.bind_queue("eu", "orders", "eu.*").value();
    ch.publish("orders", "eu.paid", "order 1").value();
    ch.publish("orders", "us.paid", "order 2").value();
    println("{}", ch.get("eu", true).value()->body);
    println("{}", bool(ch.get("eu", true).value()));
}
```

Output:

```text
order 1
false
```

## See also

- [unbind_queue](unbind_queue.md)
- [declare_exchange](declare_exchange.md)
- [channel](README.md)
