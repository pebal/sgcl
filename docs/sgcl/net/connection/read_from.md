[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::read_from, async_read_from

```cpp
expected<size_t, io::error> read_from(const io::file& f) const;                         // (1)
async::task<expected<size_t, io::error>> async_read_from(io::file f) const noexcept;    // (2)
```

Writes the file `f` from its position to its end to the connection, as one write: Go's `ReaderFrom` of a
`*net.TCPConn`, and what [io::copy](../../io/copy.md)`(connection, file)` calls. Over TCP the file goes by
`sendfile`, its pages to the socket with no copy through the process; over TLS it is read in blocks of 32 KB and
sealed where it lies; over a pair in memory, in blocks written in turn. The file's position moves past the bytes
sent, and the write deadline holds. A file that is not a regular one (a pipe) is copied as `io::copy` copies any
reader.

1. On the calling thread.
2. The same for a task, which holds no worker while it waits; the task holds the file.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the file to send, from its position |

## Return value

The number of bytes sent, fewer than the rest of the file when it ended first; or the
[io::error](../../io/error.md) of the [write](write.md), or of the read of the file.

## Complexity

Linear in the bytes sent.

## Exceptions

- (1) `std::system_error` when the write has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> send_page(net::listener l) {
    net::connection c = co_await l.async_accept();
    io::file page = io::open("page.html");
    println("sent {}", (co_await c.async_read_from(page)).value());
    println("position {}", page.tell().value());
    c.close();
}

int main() {
    io::write_file("page.html", "<h1>hello</h1>\n");
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(send_page(l));
    net::connection c = net::tcp::connect(l.local_endpoint());
    print("{}", c.read_all_text().value());
    server.wait();
    c.close();
}
```

Output:

```text
sent 15
position 15
<h1>hello</h1>
```

## See also

- [write, async_write](write.md): bytes and text
- [copy_to](copy_to.md): another connection to its end
- [io::copy](../../io/copy.md): any reader into any writer
- [sgcl::net::connection](../connection.md)
