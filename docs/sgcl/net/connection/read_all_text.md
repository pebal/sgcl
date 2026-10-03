[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::read_all_text, async_read_all_text

```cpp
expected<string, io::error> read_all_text() const;                                // (1)
async::task<expected<string, io::error>> async_read_all_text() const noexcept;    // (2)
```

Reads everything to the end of the stream into a `string`: [read_all](read_all.md) with the bytes as text, which
is [io::read_all_text](../../io/read_all_text.md) over the connection.

1. On the calling thread.
2. The same for a task, which holds no worker while it waits.

## Parameters

None.

## Return value

The text, empty when the stream was at its end; or the [io::error](../../io/error/README.md) of the [read](read.md) that
failed.

## Complexity

Linear in the bytes read.

## Exceptions

- (1) `std::system_error` when a read has to wait and the thread of the reactor, or of the timers, which its first
  use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> greet(net::listener l) {
    net::connection c = co_await l.async_accept();
    co_await c.async_write("hello from the server");
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(greet(l));
    net::connection c = net::tcp::connect(l.local_endpoint());
    println("{}", c.read_all_text().value());
    server.wait();
    c.close();
}
```

Output:

```text
hello from the server
```

## See also

- [read_all](read_all.md): the same into bytes
- [read_line](read_line.md): a line at a time
- [sgcl::net::connection](README.md)
