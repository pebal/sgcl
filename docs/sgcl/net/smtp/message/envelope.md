[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::envelope

```cpp
const smtp::envelope& envelope() const noexcept;
```

The [envelope](../envelope.md) of the message: MAIL's address and parameters, the recipients taken, the client's address and EHLO name, TLS, the authenticated user.

## Parameters

None.

## Return value

The envelope, a reference to the message's own.

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
    srv.handle([](net::smtp::message m) {
        auto& e = m.envelope();
        println("{} {} {} {}", e.from, e.to[0], e.helo.empty(), e.tls);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    encoding::email sent("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");
    net::smtp::send(url, sent);
    srv.close();
    serving.wait();
}
```

Output:

```text
alice@example.com bob@example.org false false
```

## See also

- [envelope](../envelope.md)
- [message](README.md)
