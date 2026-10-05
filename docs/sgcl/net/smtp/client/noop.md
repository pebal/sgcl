[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::noop, async_noop

```cpp
expected<void, io::error> noop() const;
async::task<expected<void, io::error>> async_noop() const noexcept;
```

NOOP: the server asked whether it is there, and the session kept from its idle timeout.

`noop` waits on the calling thread; a task awaits `async_noop`.

## Parameters

None.

## Return value

Nothing, or the error.

## Complexity

One round trip.

## Exceptions

- `noop`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_noop`: none.

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
    println("{}", c.noop().has_value());
    c.close();
    println("{}", c.noop().error().is_closed());
    srv.close();
    serving.wait();
}
```

Output:

```text
true
true
```

## See also

- [client](README.md)
