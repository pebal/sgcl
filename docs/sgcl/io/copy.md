[sgcl](../README.md) › [io](README.md)

# sgcl::io::copy, async_copy

```cpp
#include "sgcl/io/functions.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<req::writer W, req::reader R>
    expected<size_t, error> copy(W&& w, R&& r) noexcept(/* see below */);                       // (1)
    template<req::async_writer W, req::async_reader R>
    async::task<expected<size_t, error>> async_copy(W&& w, R&& r) noexcept(/* see below */);    // (2)
}
```

Copies the stream `r` from its position to its end into the stream `w`, as Go's `io.Copy`; Go's `io.CopyN` is
`copy(w, limit_reader(r, n))` ([limit_reader](limit_reader.md)). The destination comes first, as in Go and as
`memcpy` has it.

1. Copies on the calling thread, `config::io_copy_buffer_size` (32 KB) at a time through one block on the stack of
   the call: a read of `r` into it, a write of what was read to `w`, until a read gives 0.
2. The same for a task, through `async_read` and `async_write`. Its block is managed: a read of a task may run on
   the blocking pool, and the slice it is given holds the block. A stream given as a temporary is moved into the
   task's frame; one given by reference is the caller's to keep alive until the task is done, which
   `co_await io::async_copy(w, r)` in one statement does by itself.

Where one of the two has a way of its own, the copy is that, in one call, and no block is used: a reader's
`write_to(w)` (`async_write_to` for (2)), as Go's `WriterTo` — an [io::buffer](buffer.md) hands over what it holds in
one write — or else a writer's `read_from(r)` (`async_read_from`), as Go's `ReaderFrom`: a
[net::connection](../net/connection.md) given an `io::file` sends a regular file from its position to its end by
`sendfile` over TCP, with no copy through the process (over TLS the file is read in blocks and sealed where they
lie), and the file's position moves past the bytes sent; a pipe goes through the block as any reader does.

`r` is any [reader](req/reader.md) and `w` any [writer](req/writer.md) (2: an async reader and an async writer):
an `async_copy` of a stream that has only the blocking half does not compile. A class that carries
[mixin::reader](mixin/reader.md) has the same as [r.copy_to(w)](mixin/reader/copy_to.md), one that carries
[mixin::writer](mixin/writer.md) as [w.copy_from(r)](mixin/writer/copy_from.md).

## Parameters

| Parameter | Description |
|---|---|
| `w` | the stream to write to |
| `r` | the stream to read, to its end |

## Return value

The number of bytes copied, or the first error of a read of `r` or a write to `w`, as it gave it; what was written
before it stays written.

## Complexity

Linear in the number of bytes copied: a read and a write per 32 KB, or the one call of a stream's own way.

## Exceptions

- (1) What the read of `r` and the write of `w` throw, or the stream's own `write_to` or `read_from`; none when they
  are noexcept, and then `copy` is declared noexcept.
- (2) What the moves of streams given as temporaries into the task's frame throw; none for streams given by
  reference. What a read or a write throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer source("copied whole\n");
    auto n = io::copy(io::stdout, source);
    println("{} bytes", *n);

    // Go's io.CopyN: the first n bytes, through a limit_reader
    io::copy(io::stdout, io::limit_reader(io::buffer("first five, not more"), 5));
    println();

    io::buffer out;
    auto m = io::async_copy(out, io::buffer("by a task")).wait();
    println("{} bytes: {}", *m, out.text());
}
```

Output:

```text
copied whole
13 bytes
first
9 bytes: by a task
```

## See also

- [mixin::reader::copy_to](mixin/reader/copy_to.md), [mixin::writer::copy_from](mixin/writer/copy_from.md): the same
  as a member of a stream
- [limit_reader](limit_reader.md), [tee_reader](tee_reader.md), [multi_writer](multi_writer.md): streams over
  other streams
- [discard](README.md): the writer that keeps nothing, `io::copy(io::discard, r)`
- [read_all](read_all.md): everything to the end, kept
