[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::delete_queue, async_delete_queue

```cpp
expected<uint32_t, io::error> delete_queue(const string& queue, bool if_unused = false,                 // (1)
                                           bool if_empty = false) const;
async::task<expected<uint32_t, io::error>> async_delete_queue(string queue, bool if_unused = false,     // (2)
                                                              bool if_empty = false) const noexcept;
```

queue.delete: the queue, its messages and its bindings removed; its consumers are told (RabbitMQ's cancel
notification: their receive fails with `errc::not_found`). With `if_unused`, only without consumers; with
`if_empty`, only without messages. A queue that is not there is deleted already (RabbitMQ answers so).

`delete_queue` waits on the calling thread; a task awaits `async_delete_queue`.

## Parameters

| Parameter | Description |
|---|---|
| `queue` | the queue |
| `if_unused` | refused (406) while it has consumers |
| `if_empty` | refused (406) while it has messages |


## Return value

How many messages it held; `errc::precondition_failed`.

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
    ch.declare_queue("temp").value();
    ch.publish("", "temp", "x").value();
    println("{}", ch.delete_queue("temp").value());
}
```

Output:

```text
1
```

## See also

- [declare_queue](declare_queue.md)
- [channel](README.md)
