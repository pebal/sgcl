[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::email

```cpp
expected<encoding::email, encoding::error> email() const;                                    // (1)
expected<encoding::email, encoding::error> email(const encoding::email::limits& l) const;    // (2)
```

The message parsed, as [encoding::email::parse](../../../encoding/email/parse.md) parses one: (1) within the default limits, (2) within `l`.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the limits |

## Return value

The [email](../../../encoding/email/README.md), or the [error](../../../encoding/error/README.md) of a limit.

## Complexity

Linear in the size of the message.

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
        auto mail = m.email();
        println("{} | {} | {}", mail->from()->addr(), mail->subject(), mail->text().trim());
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
alice@example.com | Hello | Hi, Bob.
```

## See also

- [encoding::email](../../../encoding/email/README.md)
- [message](README.md)
