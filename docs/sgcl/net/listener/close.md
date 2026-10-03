[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::listener::close

```cpp
expected<void, io::error> close() const noexcept;
```

Stops listening, from any thread or task: Go's `Listener.Close`. The accepts in progress end with
`io::errc::closed`, an accept waiting out a lack of descriptors included, and no connection is accepted after it;
the connections accepted before are not touched. A unix listener removes its socket's file, as in Go. A second
close does nothing and succeeds. A listener not closed is closed by its destructor, on the collector's thread after
the sweep that finds it dead.

A TLS listener ([tls::listen](../tls/listen.md)) also stops its loop of handshakes and closes the connections that
were ready and not taken.

## Parameters

None.

## Return value

Nothing, or the [io::error](../../io/error.md) of the close, its operation `close`.

## Complexity

Constant: one system call, and the removal of a unix listener's file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> wait_for_one(net::listener l) {
    auto c = co_await l.async_accept();  // nobody connects
    println("{}", c.error().is_closed());
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto waiting = async::spawn(wait_for_one(l));
    async::sleep(10ms).wait();
    l.close();
    waiting.wait();
    println("{} {}", static_cast<bool>(l.close()), l.is_closed());
}
```

Output:

```text
true
true true
```

## See also

- [accept, async_accept](accept.md): what the close ends
- [is_closed](is_closed.md): whether the listener was closed
- [sgcl::net::listener](../listener.md)
