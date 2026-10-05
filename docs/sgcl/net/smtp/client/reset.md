[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::reset, async_reset

```cpp
expected<void, io::error> reset() const;
async::task<expected<void, io::error>> async_reset() const noexcept;
```

RSET: the transaction the server holds dropped. A send resets by itself after a refusal; this is for a program that drives the session itself.

`reset` waits on the calling thread; a task awaits `async_reset`.

## Parameters

None.

## Return value

Nothing, or the error: `smtp_reply`, the connection's.

## Complexity

One round trip.

## Exceptions

- `reset`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_reset`: none.

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
    println("{}", c.reset().has_value());
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [client](README.md)
