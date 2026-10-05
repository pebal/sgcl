[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::quit, async_quit

```cpp
expected<void, io::error> quit() const;
async::task<expected<void, io::error>> async_quit() const noexcept;
```

QUIT (not sent on a session already broken), then the connection closed. Every later call is `io::errc::closed`.

`quit` waits on the calling thread; a task awaits `async_quit`.

## Parameters

None.

## Return value

Nothing, or the error of the close.

## Complexity

One round trip.

## Exceptions

- `quit`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_quit`: none.

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
    println("{}", c.quit().has_value());
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

- [close](close.md)
- [client](README.md)
