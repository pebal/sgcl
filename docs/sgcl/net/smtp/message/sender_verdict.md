[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::sender_verdict

```cpp
const optional<smtp::sender_verdict>& sender_verdict() const noexcept;
```

What the server's checks found for the message: [SPF](../../spf/result.md), [DKIM](../../dkim/result.md) and
[DMARC](../../dmarc/result.md), made after DATA and before the handler when the
[server](../server/README.md)'s `sender_checks` is set; nullopt when it is not.

## Parameters

None.

## Return value

The [sender_verdict](../sender_verdict/README.md), or nullopt.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.handle([](net::smtp::message m) {
        println("{}", m.sender_verdict().has_value());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::smtp::send(string::concat("smtp://", l.local_endpoint().to_string()),
                    encoding::email("a@example.com", "b@example.org", "Hi", "Hello."));
    srv.close();
    serving.wait();
}
```

Output:

```text
false
```

## See also

- [sender_verdict](../sender_verdict/README.md)
- [server](../server/README.md)
- [message](README.md)
