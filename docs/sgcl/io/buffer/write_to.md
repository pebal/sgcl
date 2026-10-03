[sgcl](../../README.md) › [io](../README.md) › [buffer](../buffer.md)

# sgcl::io::buffer::write_to, async_write_to

```cpp
/*(1)*/ template<class W>
        expected<size_t, error> write_to(W& w) const noexcept(/* see below */);
/*(2)*/ template<class W>
        async::task<expected<size_t, error>> async_write_to(W& w) const noexcept;
```

Writes everything the buffer holds to `w` in one write, and takes the bytes written from its front when the write
succeeds, as a read takes them; when it fails, the bytes stay. The buffer itself as `w` appends a copy of what it
holds and keeps that copy, as Go's `io.Copy(b, b)` does: `io::copy(b, b)` leaves `b` as it was. It is what
[copy](../copy.md) calls with a buffer as the source, in place of its loop of reads, as Go's `io.Copy` calls a
`WriterTo`. `w` is any writer ([req::writer](../req/writer.md)): a stream with `write`, or a callable.

1. Writes with `w`'s `write`. Noexcept when that write is.
2. Returns a task that writes with `w`'s `async_write`. The writer is the caller's to keep alive until the task is
   done, as `async_copy`'s frame keeps it.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the writer |

## Return value

The number of bytes written, all the buffer held; or the writer's error.

## Complexity

One write of the bytes held.

## Exceptions

- (1) What `w`'s write throws; none when it is noexcept.
- (2) None. What the write throws is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer report;
    report.write("line one\n");
    report.write("line two\n");
    auto n = report.write_to(io::stdout);
    println("{} bytes in one write, {} left", *n, report.size());
}
```

Output:

```text
line one
line two
18 bytes in one write, 0 left
```

## See also

- [copy](../copy.md): what calls it
- [copy_to](../mixin/reader/copy_to.md): the same for any reader
- [sgcl::io::buffer](../buffer.md)
