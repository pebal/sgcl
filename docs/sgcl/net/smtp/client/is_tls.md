[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::is_tls

```cpp
bool is_tls() const noexcept;
```

Whether the session runs over TLS: `smtps://`, or after STARTTLS.

## Parameters

None.

## Return value

`true` or `false`.

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
    srv.hostname = "mx.example";
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    net::smtp::client c = net::smtp::client::connect(url);
    println("{}", c.is_tls());
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
false
```

## See also

- [client](README.md)
