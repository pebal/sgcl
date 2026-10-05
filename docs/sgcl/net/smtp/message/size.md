[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [message](README.md)

# sgcl::net::smtp::message::size

```cpp
size_t size() const noexcept;
```

The size of the message in bytes, as received (its dots taken off).

## Parameters

None.

## Return value

The size.

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
        println("{}", m.size() == m.bytes().size());
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
true
```

## See also

- [message](README.md)
