[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [client](README.md)

# sgcl::net::nats::client::publish, async_publish

```cpp
expected<void, io::error> publish(const string& subject, const string& data) const;                                // (1)
async::task<expected<void, io::error>> async_publish(const string& subject, const string& data) const noexcept;    // (2)
expected<void, io::error> publish(const message& m) const;                                                         // (3)
async::task<expected<void, io::error>> async_publish(const message& m) const noexcept;                             // (4)
```

PUB, or HPUB with headers: the data to the subject, for every subscription that matches it. Buffered and written
with the publications beside it; the call does not wait for the server (NATS's core is at most once):
[flush](flush.md) waits until the server read it.

- (1–2) The data to the subject.
- (3–4) A whole [message](../message/README.md): its reply subject and its headers.

## Parameters

| Parameter | Description |
|---|---|
| `subject` | the subject; no wildcard |
| `data` | the bytes |
| `m` | the message |


## Return value

Nothing; `errc::invalid_subject` for a subject with a wildcard or white space, `errc::max_payload` past the
server's limit, the connection's end.

## Complexity

Linear in the data.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription sub = nc.subscribe("orders").value();
    net::nats::message m("orders", "{\"id\":7}");
    m.headers = {{"Content-Type", "application/json"}};
    nc.publish(m).value();
    auto got = sub.receive().value();
    println("{} {}", got.header("content-type"), got.data);
}
```

Output:

```text
application/json {"id":7}
```

## See also

- [message](../message/README.md)
- [flush](flush.md)
- [client](README.md)
