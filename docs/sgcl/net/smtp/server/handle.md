[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [server](README.md)

# sgcl::net::smtp::server::handle

```cpp
template<class Handler>
server& handle(Handler h);
```

The handler of each message: a function of a [message](../message/README.md), called once the message is whole,
in the session's task, the reply to the message's end what it returns:

- `void`: accepted, `250 2.0.0 OK: message accepted`;
- `smtp::reply`: its code and text, a code of 400 or more refusing the message, a code of 0 accepting it as void does;
- `async::task<>` or `async::task<smtp::reply>`: the same for a handler that waits (stores the message, asks
  another service).

A handler set again replaces the one before. A handler that throws answers 451.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handler |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.handle([](net::smtp::message m) -> async::task<net::smtp::reply> {
        if (m.size() > 1000) {
            co_return net::smtp::reply{552, "5.3.4", "Too big for us"};
        }
        co_return net::smtp::reply{250, "2.0.0", "Queued as 42"};
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    encoding::email m("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");
    println("{}", net::smtp::send(url, m)->reply.to_string());
    encoding::email big("a@example.com", "b@example.org", "Big", string(5000, 'x'));
    println("{}", net::smtp::send(url, big).error().message());
    srv.close();
    serving.wait();
}
```

Output:

```text
250 2.0.0 Queued as 42
BDAT 552 5.3.4 Too big for us: the SMTP server refused
```

## See also

- [message](../message/README.md)
- [server](README.md)
