[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::read_line, async_read_line

```cpp
expected<optional<string>, io::error> read_line() const;                                // (1)
async::task<expected<optional<string>, io::error>> async_read_line() const noexcept;    // (2)
```

Reads the next line, without its `"\n"` or `"\r\n"`: Go's `bufio.NewReader(c).ReadString('\n')` without the reader
to keep. The last line of the stream needs no `"\n"`; `nullopt` is the end of the stream.

The first call puts a buffer of 8 KB in front of the connection, which [read](read.md) and the functions over it
take from first from then on, so that lines and bytes read in turns lose nothing. A line is bounded, 64 KB by
default ([set_max_line](set_max_line.md)): the input is the network's.

1. On the calling thread.
2. The same for a task, which holds no worker while it waits.

## Parameters

None.

## Return value

The line, empty for an empty line; `nullopt` at the end of the stream. Or the [io::error](../../io/error.md):

- `io::errc::line_too_long`, its operation `read_line`, when the line passes [max_line](max_line.md);
- the error of the [read](read.md) underneath otherwise.

## Complexity

Linear in the length of the line; one read per 8 KB that comes.

## Exceptions

- (1) `std::system_error` when a read has to wait and the thread of the reactor, or of the timers, which its first
  use starts, cannot be made.
- (2) None.

## Notes

A line too long is consumed when its end has come into the buffer: the next call reads the line after it. One whose
end has not come is not consumed, and the connection is then of no use but to close. The rules are those of
[buffered_reader::read_line](../../io/buffered_reader/read_line.md), which a connection keeps inside it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto [a, b] = net::connection::in_memory();
    auto writing = async::spawn([a]() -> async::task<> {
        co_await a.async_write("GET / HTTP/1.1\r\nHost: example\r\n\r\nbody");
        a.close_write();
    });
    for (;;) {
        optional<string> line = b.read_line().value();
        if (!line) {
            break;
        }
        println("[{}]", *line);
    }
    writing.wait();
}
```

Output:

```text
[GET / HTTP/1.1]
[Host: example]
[]
[body]
```

## See also

- [set_max_line](set_max_line.md): the bound of a line
- [read, async_read](read.md): bytes, taken from the line's buffer first
- [buffered_reader](../../io/buffered_reader.md): lines over any reader
- [sgcl::net::connection](../connection.md)
