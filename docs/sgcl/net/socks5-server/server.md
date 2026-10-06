[sgcl](../../README.md) › [net](../README.md) › [socks5](../socks5/README.md) › [server](README.md)

# sgcl::net::socks5::server::server

```cpp
server();                       // (1)
server(const server& other);    // (2)
```

1. A server with no connections, its fields at their defaults.
2. The same server as `other`: its connections shared, its fields copied.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the server copied |


## Return value

None.

## Complexity

Constant.

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
    net::socks5::server copy = proxy;
    println("{}", copy.connections());
    proxy.close();
    serving.wait();
}
```

Output:

```text
0
```

## See also

- [serve](serve.md)
- [server](README.md)
