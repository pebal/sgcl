[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::close

```cpp
expected<void, io::error> close() const noexcept;
```

Closes the socket, from any thread or task: Go's `Close`. The receives and sends in progress in other tasks end
with `io::errc::closed`, and no operation starts after it; a second close does nothing and succeeds. The descriptor
goes back to the system when the last operation in progress has let go of it, never under one. A socket not
closed is closed by its destructor, on the collector's thread after the sweep that finds it dead: later than the
last use. A close never waits, so it has no `async_` form.

## Parameters

None.

## Return value

Nothing, or the [io::error](../../io/error/README.md) of the close, its operation `close`.

## Complexity

Constant: one system call, or none when an operation in progress makes it.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> wait_for_one(net::udp::socket s) {
    vector<byte> room(64);
    auto d = co_await s.async_receive_from(room);  // nobody sends
    println("{} {}", d.error().is_closed(), d.error().op());
}

int main() {
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    auto waiting = async::spawn(wait_for_one(s));
    async::sleep(10ms).wait();
    s.close();
    waiting.wait();
    println("{} {}", static_cast<bool>(s.close()), s.is_closed());
}
```

Output:

```text
true read
true true
```

## See also

- [is_closed](is_closed.md): whether the socket was closed
- [set_deadline](set_deadline.md): an end to the waits without a close
- [sgcl::net::udp::socket](README.md)
