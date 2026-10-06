[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [broker](README.md)

# sgcl::net::mqtt::broker::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (2)
expected<void, io::error> serve(const net::listener& l) const;                                // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

- (1–2) Listens on the address (`":1883"`) and serves its connections, each a task of the scheduler, until
  [shutdown](shutdown.md) or [close](close.md).
- (3–4) The connections of a listener the program made; a TLS listener's are TLS.

`serve` blocks the calling thread; a task awaits `async_serve`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen |
| `l` | the listener |

## Return value

`net::errc::server_closed` after a shutdown or a close; the listen's error; an accept's error.

## Complexity

Each connection a task.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(b.async_serve(l));
    string url = string::concat("mqtt://", l.local_endpoint().to_string());
    b.close();
    println("{}", serving.wait().error().code() == net::errc::server_closed);
}
```

Output:

```text
true
```

## See also

- [serve_tls](serve_tls.md), [accept](accept.md)
- [broker](README.md)
