[sgcl](../README.md) › [io](README.md)

# sgcl::io::tee_reader

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class tee_reader;   // : public mixin::reader<tee_reader>
}
```

`sgcl::io::tee_reader` is a reader that writes what it reads to a writer as well: what Go's `io.TeeReader` is. A
stream read through it is copied on the way, into a hash, a log or a buffer kept for a second look, with no pass of
its own. An error of the write is the read's error. The standard library has no counterpart.

## Rules

- It holds an [io::reader](reader.md) and an [io::writer](writer.md), `tracked_ptr`s, so it lives where one may:
  on a stack, in a task, in a managed object ([The rules](../core/README.md#the-rules), 1).
- An object, not a handle: given by reference to an `io::reader` or to `io::copy`, it is referenced, and the caller
  keeps it alive; given as a temporary, it is copied into a managed object of its own.
- [async_read](tee_reader/read.md) is over the source's own `async_read` and the writer's own `async_write`; a
  stream that has only the blocking half has it run on the [blocking pool](../async/spawn_blocking.md) by its handle.
- One thread or task at a time, as on any stream.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](tee_reader/tee_reader.md) | constructs the reader over a source and a writer |
| [read, async_read](tee_reader/read.md) | reads bytes of the source and writes them to the writer |

#### From mixin::reader

The rest of what a reader does, each an algorithm of io over this stream ([mixin::reader](mixin/reader.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](mixin/reader/read_full.md) | fills the whole buffer |
| [read_all, async_read_all](mixin/reader/read_all.md) | everything to the end of the source, as bytes |
| [read_all_text, async_read_all_text](mixin/reader/read_all_text.md) | everything to the end of the source, as a string |
| [copy_to, async_copy_to](mixin/reader/copy_to.md) | the source to its end, written to a writer |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer seen;
    io::tee_reader input(io::buffer("first\nsecond\n"), seen);
    io::buffered_reader lines(input);
    auto first = lines.read_line();
    println("read: {}", **first);
    println("seen so far: {} bytes", seen.size());  // the block the line came from
}
```

Output:

```text
read: first
seen so far: 13 bytes
```

## See also

- [multi_writer](multi_writer.md): one write to several writers
- [limit_reader](limit_reader.md), [multi_reader](multi_reader.md), [transform_reader](transform_reader.md): the
  other readers over readers
- [reader](reader.md), [writer](writer.md)
