[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::limit_reader

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class limit_reader;   // : public mixin::reader<limit_reader>
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::limit_reader` is a reader of the first `n` bytes of another reader, then the end: what Go's
`io.LimitReader` is. `io::copy(w, io::limit_reader(r, n))` is Go's `io.CopyN`, and a decoder given a member of an
archive or the body of a message reads no further than its length. The source keeps what the limit did not read:
the next read of it starts at byte `n`. The standard library has no counterpart (`std::istream::read` takes a count
for one call, not for a stream).

## Rules

- An object, not a handle: a copy reads the same source with a count of its own. Given by reference to an
  `io::reader` or to `io::copy`, it is referenced, and the caller keeps it alive; given as a temporary, it is copied
  into a managed object of its own.
- [async_read](read.md) is over the source's own `async_read`; for a source that has only `read`, the
  source's `io::reader` runs it on the [blocking pool](../../async/spawn_blocking.md).
- One thread or task at a time, as on any stream.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](limit_reader.md) | constructs the reader over a source and a count |
| [read, async_read](read.md) | reads bytes of the source, no more than remain |
| [remaining](remaining.md) | the number of bytes the limit still lets through |

#### From mixin::reader

The rest of what a reader does, each an algorithm of io over this stream ([mixin::reader](../mixin/reader/README.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills the whole buffer |
| [read_all, async_read_all](../mixin/reader/read_all.md) | everything to the limit, as bytes |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | everything to the limit, as a string |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | the stream to the limit, written to a writer |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer records("5hello7goodbye");  // each record: its length in one digit, then the text
    while (!records.empty()) {
        vector<byte> length(1);
        records.read_full(length);
        auto text = io::limit_reader(records, uint64_t(length[0]) - '0').read_all_text();
        println("[{}]", *text);
    }
}
```

Output:

```text
[hello]
[goodbye]
```

## See also

- [copy](../copy.md): a reader to its end into a writer
- [read_full](../read_full.md): exactly as many bytes as a buffer holds
- [multi_reader](../multi_reader/README.md), [tee_reader](../tee_reader/README.md), [transform_reader](../transform_reader/README.md): the other
  readers over readers
- [reader](../reader/README.md)
