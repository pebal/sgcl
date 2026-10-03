[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::remote_endpoint

```cpp
endpoint remote_endpoint() const noexcept;
```

Returns the peer of a socket of [udp::connect](../udp/connect.md): Go's `RemoteAddr`. A socket of
[udp::bind](../udp/bind.md) has none: the [endpoint](../endpoint/README.md) is empty (`!is_valid()`).

## Parameters

None.

## Return value

The address and port of the peer, or an empty endpoint.

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
    net::udp::socket bound = net::udp::bind("127.0.0.1:0");
    println("{}", bound.remote_endpoint().is_valid());

    net::udp::socket connected = net::udp::connect("127.0.0.1:53");
    println("{}", connected.remote_endpoint());
}
```

Output:

```text
false
127.0.0.1:53
```

## See also

- [local_endpoint](local_endpoint.md): this end
- [sgcl::net::udp::socket](README.md)
