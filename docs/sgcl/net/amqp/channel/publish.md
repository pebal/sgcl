[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::publish, async_publish

```cpp
expected<void, io::error> publish(const string& exchange, const string& routing_key, const string& body,           // (1)
                                  const properties& p = {}, const publish_options& o = {}) const;
async::task<expected<void, io::error>> async_publish(string exchange, string routing_key, string body,             // (2)
                                                     properties p = {}, publish_options o = {}) const noexcept;
```

basic.publish: the message to the exchange with the routing key; "" is the default exchange, whose routing key is
a queue's name. The body is bytes; the [properties](../properties.md) go beside it. Without confirms the message is
handed to the connection's writer and the call returns (written with the ones beside it in one write); with
[confirm](confirm.md), the call waits for the broker's acknowledgement. A mandatory message no queue takes comes back
to [receive_returned](receive_returned.md).

`publish` waits on the calling thread; a task awaits `async_publish`.

## Parameters

| Parameter | Description |
|---|---|
| `exchange` | the exchange; "" the default |
| `routing_key` | the key the exchange routes by |
| `body` | the message's bytes |
| `p` | the properties |
| `o` | mandatory |


## Return value

Nothing; with confirms `errc::nacked` for a message the broker refused; the channel's end (a publication to an
exchange that is not there closes the channel: the next call has the error); `errc::syntax_error` for an exchange or
key past 255 bytes.

## Complexity

Linear in the body.

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
    ch.declare_queue("mail").value();
    net::amqp::properties p;
    p.content_type = "application/json";
    p.headers = {{"attempt", net::amqp::field(1)}};
    ch.publish("", "mail", "{\"to\":\"alice\"}", p).value();
    auto d = ch.get("mail", true).value();
    println("{} {} {}", d->properties.content_type, *net::amqp::find(d->properties.headers, "attempt")->as_int(), d->body);
}
```

Output:

```text
application/json 1 {"to":"alice"}
```

## See also

- [properties](../properties.md)
- [confirm](confirm.md)
- [consume](consume.md)
- [channel](README.md)
