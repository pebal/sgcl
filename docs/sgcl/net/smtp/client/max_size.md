[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::max_size

```cpp
uint64_t max_size() const noexcept;
```

The largest message the server's SIZE allows; a send of a larger one is refused before it is sent.

## Parameters

None.

## Return value

The bytes; 0 when the server names none.

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
    println("{}", c.max_size());
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
33554432
```

## See also

- [client](README.md)
