[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::respond, async_respond

```cpp
expected<void, io::error> respond(const message& to, const string& data) const;                                // (1)
async::task<expected<void, io::error>> async_respond(const message& to, const string& data) const noexcept;    // (2)
```

A reply to a message received: the data to its reply subject, written at once.

`respond` waits on the calling thread; a task awaits `async_respond`.

## Parameters

| Parameter | Description |
|---|---|
| `to` | the message replied to |
| `data` | the reply's bytes |


## Return value

Nothing; `errc::invalid_subject` for a message without a reply subject, the connection's end.

## Complexity

Linear in the data.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription echo = nc.subscribe("echo").value();
    auto server = async::spawn([](net::nats::client nc, net::nats::subscription echo) -> async::task<> {
        auto m = co_await echo.async_receive();
        co_await nc.async_respond(*m, m->data);
    }(nc, echo));
    println("{}", nc.request("echo", "ping").value().data);
    server.wait();
}
```

Output:

```text
ping
```

## See also

- [request](request.md)
- [client](README.md)
