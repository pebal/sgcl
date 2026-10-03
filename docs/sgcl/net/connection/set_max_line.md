[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::set_max_line

```cpp
void set_max_line(size_t bytes) const noexcept;
```

Sets the longest line [read_line](read_line.md) accepts, in bytes, the `"\n"` not counted and a `"\r"` before it
counted, as the [buffered reader](../../io/buffered_reader/set_max_line.md) counts; 0 for no bound. The default is 64 KB: the input of a connection is the network's, and a peer must not make it hold a line
of any length. A longer line is the error `io::errc::line_too_long`, found as soon as the bytes buffered pass the
bound, before the line is assembled whole.

It takes no lock: a `read_line` in progress keeps the bound it started with, and the next one takes the new one.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the bound, in bytes; 0 for none |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto [a, b] = net::connection::in_memory();
    b.set_max_line(8);
    auto writing = async::spawn([a]() -> async::task<> {
        co_await a.async_write("short\nthis line is too long\nok again\n");
        a.close_write();
    });
    for (int i : range(3)) {
        auto line = b.read_line();
        println("{}", line ? **line : line.error().message());
    }
    writing.wait();
}
```

Output:

```text
short
read_line: line too long
ok again
```

## See also

- [max_line](max_line.md): the bound now
- [read_line, async_read_line](read_line.md): what the bound limits
- [sgcl::net::connection](../connection.md)
