[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::request, async_request

```cpp
expected<message, io::error> request(const string& subject, const string& data,                                        // (1)
                                     duration timeout = std::chrono::seconds(2)) const;
async::task<expected<message, io::error>> async_request(const string& subject, const string& data,                     // (2)
                                                        duration timeout = std::chrono::seconds(2)) const noexcept;
expected<message, io::error> request(const message& m, duration timeout = std::chrono::seconds(2)) const;              // (3)
async::task<expected<message, io::error>> async_request(const message& m,                                              // (4)
                                                        duration timeout = std::chrono::seconds(2)) const noexcept;
```

A request and its first reply: the message published with a reply subject under this client's inbox (one
subscription for every request of the connection), the reply waited for. A server of headers tells at once when
no one subscribes to the subject.

## Parameters

| Parameter | Description |
|---|---|
| `subject` | the subject of the service |
| `data` | the request's bytes |
| `m` | a whole message: its headers |
| `timeout` | how long the reply is waited for; zero: no limit |


## Return value

The reply; `errc::no_responders` when no one subscribes to the subject, `ETIMEDOUT` past the timeout, the connection's end.

## Complexity

A round trip through the responder.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription service = nc.subscribe("time.utc").value();
    auto responder = async::spawn([](net::nats::client nc, net::nats::subscription service) -> async::task<> {
        auto m = co_await service.async_receive();
        co_await nc.async_respond(*m, "12:00");
    }(nc, service));
    println("{}", nc.request("time.utc", "now?").value().data);
    responder.wait();
    auto none = nc.request("nobody.home", "x");
    println("{}", none.error().code() == net::nats::errc::no_responders);
}
```

Output:

```text
12:00
true
```

## See also

- [respond](respond.md)
- [client](README.md)
