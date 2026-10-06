[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::subscribe, async_subscribe

```cpp
expected<subscription, io::error> subscribe(const string& subject, const string& queue_group = {}) const;                  // (1)
async::task<expected<subscription, io::error>> async_subscribe(string subject, string queue_group = {}) const noexcept;    // (2)
```

SUB: the messages of the subject, "*" matching a token and ">" the rest, to a
[subscription](../subscription/README.md). With a queue group, each message goes to one member of the group, the
group's subscriptions sharing the work.

`subscribe` waits on the calling thread; a task awaits `async_subscribe`.

## Parameters

| Parameter | Description |
|---|---|
| `subject` | the subject, with wildcards |
| `queue_group` | the group's name; empty: none |


## Return value

The subscription; `errc::invalid_subject`, the connection's end. A subject the server refuses ends the subscription with `errc::permissions_violation`.

## Complexity

Constant.

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
    auto a = nc.subscribe("jobs", "workers").value();
    auto b = nc.subscribe("jobs", "workers").value();
    nc.flush().value();
    for (int i = 0; i < 10; ++i) {
        nc.publish("jobs", "job").value();
    }
    nc.flush().value();
    int n = 0;
    while (a.try_receive() || b.try_receive()) {
        ++n;
    }
    println("{}", n);
}
```

Output:

```text
10
```

## See also

- [subscription](../subscription/README.md)
- [unsubscribe](../subscription/unsubscribe.md)
- [client](README.md)
