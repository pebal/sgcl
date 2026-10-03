[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::multi_reader

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class multi_reader;   // : public mixin::reader<multi_reader>
}
```

`sgcl::io::multi_reader` is several readers one after another, as one stream: what Go's `io.MultiReader` is. A read
goes to the current reader; when that one ends, the next one is read, and the stream ends with the last. A header
in memory before a file, the parts of a message, the files of a list read as one. The standard library has no
counterpart.

## Rules

- It holds a [vector](../../core/vector/README.md) of [io::reader](../reader/README.md)s, so it lives where a `tracked_ptr` may: on a
  stack, in a task, in a managed object ([The rules](../../core/README.md#the-rules), 1).
- An object, not a handle: given by reference to an `io::reader` or to `io::copy`, it is referenced, and the caller
  keeps it alive; given as a temporary, it is copied into a managed object of its own.
- [async_read](read.md) is over each reader's own `async_read`; a reader that has only `read` has it
  run on the [blocking pool](../../async/spawn_blocking.md) by its handle.
- One thread or task at a time, as on any stream.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](multi_reader.md) | constructs the reader over a vector of readers |
| [read, async_read](read.md) | reads bytes of the current reader, then of the next |

#### From mixin::reader

The rest of what a reader does, each an algorithm of io over this stream ([mixin::reader](../mixin/reader/README.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills the whole buffer, across the readers |
| [read_all, async_read_all](../mixin/reader/read_all.md) | everything to the end of the last reader, as bytes |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | everything to the end of the last reader, as a string |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | the readers to their end, written to a writer |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer body("a,b\n1,2\n");
    io::multi_reader message({io::buffer("# a header\n"), body, io::buffer("# the end\n")});
    io::copy(io::stdout, message);
}
```

Output:

```text
# a header
a,b
1,2
# the end
```

## See also

- [multi_writer](../multi_writer/README.md): one write to several writers
- [limit_reader](../limit_reader/README.md), [tee_reader](../tee_reader/README.md), [transform_reader](../transform_reader/README.md): the other
  readers over readers
- [reader](../reader/README.md)
