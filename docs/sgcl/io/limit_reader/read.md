[sgcl](../../README.md) › [io](../README.md) › [limit_reader](../limit_reader.md)

# sgcl::io::limit_reader::read, async_read

```cpp
/*(1)*/ expected<size_t, error> read(const slice<byte>& buffer);
/*(2)*/ async::task<expected<size_t, error>> async_read(slice<byte> buffer) noexcept;
```

Reads from the source into `buffer` no more than the bytes that [remain](remaining.md), and counts down by the
bytes read. When none remain, or `buffer` is empty, the read is 0 and the source is not called.

1. Reads with the source's `read`.
2. Returns a task that reads with the source's `async_read`; for a source that has only `read`, its `io::reader`
   runs it on the [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; the read takes no more of it than remain |

## Return value

The number of bytes read, 0 at the limit or at the end of the source; or the source's error, with nothing counted.

## Complexity

That of the source's read.

## Exceptions

- (1) What the source's `read` throws.
- (2) None. What the source's read throws is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> head(io::reader source) {
    io::limit_reader first(source, 6);
    vector<byte> block(4);
    while (true) {
        auto n = co_await first.async_read(block);
        if (!n || *n == 0) {
            break;
        }
        println("{} bytes, {} remain", *n, first.remaining());
    }
}

int main() {
    async::run(head(io::buffer("a long text")));
}
```

Output:

```text
4 bytes, 2 remain
2 bytes, 0 remain
```

## See also

- [remaining](remaining.md)
- [sgcl::io::limit_reader](../limit_reader.md)
