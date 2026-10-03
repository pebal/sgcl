[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::copy_to, async_copy_to

```cpp
expected<size_t, io::error> copy_to(const connection& other) const;                                // (1)
async::task<expected<size_t, io::error>> async_copy_to(const connection& other) const noexcept;    // (2)
```

Reads this connection to the end of its stream and writes everything to `other`: Go's `io.Copy(other, c)`. An echo
is `c.copy_to(c)`, a proxy two of them, one each way. It is [io::copy](../../io/copy.md) between the two.

1. On the calling thread.
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the connection to write to; this one itself for an echo |

## Return value

The number of bytes copied; or the [io::error](../../io/error/README.md) of the [read](read.md) or the [write](write.md)
that failed, the bytes copied before it lost.

## Complexity

Linear in the bytes copied.

## Exceptions

- (1) `std::system_error` when a read or a write has to wait and the thread of the reactor, or of the timers, which
  its first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> echo(net::listener l) {
    net::connection c = co_await l.async_accept();
    size_t n = (co_await c.async_copy_to(c)).value();  // reads to the end and writes it back
    println("echoed {}", n);
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(echo(l));
    net::connection c = net::tcp::connect(l.local_endpoint());
    c.write("echo me");
    c.close_write();
    println("{}", c.read_all_text().value());
    server.wait();
    c.close();
}
```

Output:

```text
echoed 7
echo me
```

## See also

- [read_from](read_from.md): a file into the connection
- [read_all](read_all.md): to the end, into memory
- [sgcl::net::connection](README.md)
