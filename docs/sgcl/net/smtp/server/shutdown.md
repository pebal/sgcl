[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [server](README.md)

# sgcl::net::smtp::server::shutdown, async_shutdown

```cpp
void shutdown() const;
async::task<> async_shutdown() const noexcept;
```

Gracefully: the listeners closed, the sessions waiting for a command closed, the others answered 421 after the
command they are in; returns when all of them have ended.

`shutdown` waits on the calling thread; a task awaits `async_shutdown`.

## Parameters

None.

## Return value

None.

## Complexity

As long as the sessions take.

## Exceptions

- `shutdown`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_shutdown`: none.

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
    srv.shutdown();
    println("{}", c.noop().has_value());
    serving.wait();
}
```

Output:

```text
false
```

## See also

- [close](close.md)
- [server](README.md)
