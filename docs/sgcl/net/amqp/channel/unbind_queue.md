[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::unbind_queue, async_unbind_queue

```cpp
expected<void, io::error> unbind_queue(const string& queue, const string& exchange, const string& routing_key,    // (1)
                                       const table& arguments = {}) const;
async::task<expected<void, io::error>> async_unbind_queue(string queue, string exchange, string routing_key,      // (2)
                                                          table arguments = {}) const noexcept;
```

queue.unbind: the binding of the same key and arguments removed.

`unbind_queue` waits on the calling thread; a task awaits `async_unbind_queue`.

## Parameters

| Parameter | Description |
|---|---|
| `queue` | the queue |
| `exchange` | the exchange |
| `routing_key` | the key the binding has |
| `arguments` | the arguments it has |


## Return value

Nothing; `errc::not_found`.

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
    ch.declare_exchange("news", {.type = "fanout"}).value();
    ch.declare_queue("reader").value();
    ch.bind_queue("reader", "news", "").value();
    ch.unbind_queue("reader", "news", "").value();
    ch.publish("news", "", "unheard").value();
    println("{}", bool(ch.get("reader", true).value()));
}
```

Output:

```text
false
```

## See also

- [bind_queue](bind_queue.md)
- [channel](README.md)
