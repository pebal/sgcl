[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::purge_queue, async_purge_queue

```cpp
expected<uint32_t, io::error> purge_queue(const string& queue) const;                         // (1)
async::task<expected<uint32_t, io::error>> async_purge_queue(string queue) const noexcept;    // (2)
```

queue.purge: the queue's ready messages dropped; those delivered and not acknowledged stay.

`purge_queue` waits on the calling thread; a task awaits `async_purge_queue`.

## Parameters

| Parameter | Description |
|---|---|
| `queue` | the queue |


## Return value

How many were dropped; `errc::not_found`.

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
    ch.declare_queue("scratch").value();
    ch.publish("", "scratch", "a").value();
    ch.publish("", "scratch", "b").value();
    println("{}", ch.purge_queue("scratch").value());
}
```

Output:

```text
2
```

## See also

- [delete_queue](delete_queue.md)
- [channel](README.md)
