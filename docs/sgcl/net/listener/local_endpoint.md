[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::listener::local_endpoint

```cpp
endpoint local_endpoint() const noexcept;
```

Returns the address and port the listener listens on: Go's `Listener.Addr`. A port 0 given (`"127.0.0.1:0"`) is
the port the system chose, the one to connect to; no host given (`":0"`) is the IPv6 wildcard `::`, one socket for
both families. A unix listener has none: the [endpoint](../endpoint.md) is empty, its [path](path.md) names it.

## Parameters

None.

## Return value

The address and port, or an empty endpoint.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    net::endpoint at = l.local_endpoint();
    println("{} {}", at.address(), at.port() != 0);

    net::connection c = net::tcp::connect(at);  // the port the system chose
    println("{}", c.remote_endpoint() == at);

    net::listener both = net::tcp::listen(":0");
    println("{}", both.local_endpoint().address());
}
```

Output:

```text
127.0.0.1 true
true
::
```

## See also

- [tcp::listen](../tcp/listen.md): what the address given means
- [path](path.md): a unix listener's name
- [sgcl::net::listener](../listener.md)
