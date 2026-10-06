[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::declare_queue, async_declare_queue

```cpp
expected<queue_info, io::error> declare_queue(const string& name = {}, const queue_options& o = {}) const;                  // (1)
async::task<expected<queue_info, io::error>> async_declare_queue(string name = {}, queue_options o = {}) const noexcept;    // (2)
```

queue.declare: the queue made with the [options](../queue_options.md), or found as it is; with `passive`, only
checked. An empty name asks the broker to name the queue ("amq.gen-..."), as a reply queue is made, exclusive to
the connection. Every queue is bound to the default exchange by its name.

`declare_queue` waits on the calling thread; a task awaits `async_declare_queue`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the queue; empty: the broker names it |
| `o` | passive, durable, exclusive, auto-delete, the arguments |


## Return value

The queue's [name, messages and consumers](../queue_info.md); `errc::not_found` for a passive one that is not there, `errc::resource_locked` for another connection's exclusive queue.

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
    auto q = ch.declare_queue("tasks", {.durable = true}).value();
    ch.publish("", "tasks", "one").value();
    println("{} {}", q.name, ch.declare_queue("tasks", {.passive = true})->messages);
    auto reply = ch.declare_queue("", {.exclusive = true}).value();
    println("{}", reply.name.view().starts_with("amq.gen-"));
}
```

Output:

```text
tasks 1
true
```

## See also

- [queue_options](../queue_options.md)
- [queue_info](../queue_info.md)
- [channel](README.md)
