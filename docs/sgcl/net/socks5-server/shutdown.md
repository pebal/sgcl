[sgcl](../../README.md) › [net](../README.md) › [socks5](../socks5/README.md) › [server](README.md)

# sgcl::net::socks5::server::shutdown, async_shutdown

```cpp
void shutdown() const;                            // (1)
async::task<> async_shutdown() const noexcept;    // (2)
```

Gracefully: the listeners closed, the relays in progress left to end by themselves; returns when they have.

`shutdown` waits on the calling thread; a task awaits `async_shutdown`.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    proxy.shutdown();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
true
```

## See also

- [close](close.md)
- [server](README.md)
