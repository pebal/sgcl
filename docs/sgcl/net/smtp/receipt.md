[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md) › receipt

# sgcl::net::smtp::receipt

```cpp
#include "sgcl/net/smtp/envelope.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct receipt {
        smtp::reply reply;
        vector<rejection> rejected;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::receipt` is what a message's sending gives: the server's reply to its end
(`250 2.0.0 Ok: queued as 4ZCq1x`, the queue id in its text), and the recipients it refused while it took the
others (a sending all of whose recipients were refused is the error instead).

## Member objects

| Member | Description |
|---|---|
| `reply` | the [reply](reply/README.md) to the data's end |
| `rejected` | the [rejections](rejection.md), in the order of the recipients |

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
    srv.on_recipient = [](const net::smtp::envelope&, const string& to) {
        if (to.view().starts_with("bad")) {
            return net::smtp::reply{550, "5.1.1", "No such user"};
        }
        return net::smtp::reply();
    };
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());

    encoding::email m("a@example.com", "good@example.org, bad@example.org", "Hi", "Hello.");
    net::smtp::receipt r = net::smtp::send(url, m).value();
    println("{}", r.reply.to_string());
    for (auto& x : r.rejected) {
        println("{}: {}", x.recipient, x.reply.to_string());
    }
    srv.close();
    serving.wait();
}
```

Output:

```text
250 2.0.0 OK: message accepted
bad@example.org: 550 5.1.1 No such user
```

## See also

- [rejection](rejection.md)
- [send](send.md)
- [smtp](README.md)
