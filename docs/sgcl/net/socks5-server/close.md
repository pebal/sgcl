[sgcl](../../README.md) › [net](../README.md) › [socks5](../socks5/README.md) › [server](README.md)

# sgcl::net::socks5::server::close

```cpp
void close() const;
```

At once: every listener and connection closed, the relays ended. The server stays closed: a `serve` after it, or one
started just before it, ends at once with `net::errc::server_closed`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::socks5::server proxy;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(proxy.async_serve(l));
    proxy.close();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
true
```

## See also

- [shutdown](shutdown.md)
- [server](README.md)
