[sgcl](../../README.md) › [net](../README.md) › [socks5](../socks5/README.md) › [server](README.md)

# sgcl::net::socks5::server::connections

```cpp
size_t connections() const noexcept;
```

The clients being served: their handshakes and relays.

## Parameters

None.

## Return value

The count.

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
    // a target: an echo of one line
    net::listener target = net::tcp::listen("127.0.0.1:0");
    auto echoing = async::spawn([](net::listener l) -> async::task<> {
        auto c = co_await l.async_accept();
        if (c) {
            (void)co_await c->async_copy_to(*c);
        }
    }(target));
    net::socks5::server proxy;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(proxy.async_serve(l));
    net::connection c = net::socks5::connect(l.local_endpoint().to_string(), target.local_endpoint().to_string()).value();
    println("{}", proxy.connections());
    c.close();
    proxy.close();
    serving.wait();
}
```

Output:

```text
1
```

## See also

- [serve](serve.md)
- [server](README.md)
