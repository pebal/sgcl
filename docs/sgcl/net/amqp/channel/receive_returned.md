[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::receive_returned, async_receive_returned

```cpp
expected<returned, io::error> receive_returned() const;                                // (1)
async::task<expected<returned, io::error>> async_receive_returned() const noexcept;    // (2)
```

The next publication the broker gave back (basic.return): a mandatory one no queue took. Waits for one; the
channel's end ends the wait. The returns are kept in the channel, 1024 at most.

`receive_returned` waits on the calling thread; a task awaits `async_receive_returned`.

## Parameters

None.

## Return value

The [returned](../returned.md) message; the channel's end.

## Complexity

Constant.

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
    ch.publish("", "nobody-listens", "lost", {}, {.mandatory = true}).value();
    auto r = ch.receive_returned().value();
    println("{} {} {}", r.reply_code, r.reply_text, r.body);
}
```

Output:

```text
312 NO_ROUTE lost
```

## See also

- [try_receive_returned](try_receive_returned.md)
- [publish_options](../publish_options.md)
- [channel](README.md)
