[sgcl](../README.md) › [io](README.md)

# sgcl::io::multi_writer

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class multi_writer;   // : public mixin::writer<multi_writer>
}
```

`sgcl::io::multi_writer` is a writer that writes to every one of several writers, in their order: what Go's
`io.MultiWriter` is. A log to the terminal and to a file, a download to a file and to a hash, with one write. The
first error stops it: the writers after the one that failed are not written to. The standard library has no
counterpart.

## Rules

- It holds a [vector](../core/vector.md) of [io::writer](writer.md)s, so it lives where a `tracked_ptr` may: on a
  stack, in a task, in a managed object ([The rules](../core/README.md#the-rules), 1).
- An object, not a handle: given by reference to an `io::writer` or to `io::copy`, it is referenced, and the caller
  keeps it alive; given as a temporary, it is copied into a managed object of its own.
- [async_write](multi_writer/write.md) is over each writer's own `async_write`; a writer that has only `write` has
  it run on the [blocking pool](../async/spawn_blocking.md) by its handle.
- One thread or task at a time, as on any stream.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](multi_writer/multi_writer.md) | constructs the writer over a vector of writers |
| [write, async_write](multi_writer/write.md) | writes bytes to every writer |

#### From mixin::writer

The rest of what a writer does, each an algorithm of io over this stream ([mixin::writer](mixin/writer.md)).

| Function | Description |
|---|---|
| [write, async_write](mixin/writer/write.md) | writes a text or one byte to every writer |
| [copy_from, async_copy_from](mixin/writer/copy_from.md) | everything from a reader to its end, written to every writer |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer log;
    io::multi_writer both({io::stdout, log});
    both.write("started\n");
    both.write("done\n");
    println("the log holds {} bytes", log.size());
}
```

Output:

```text
started
done
the log holds 13 bytes
```

## See also

- [tee_reader](tee_reader.md): what is read, written to a writer as well
- [discard_writer](discard_writer.md): the writer that drops everything
- [writer](writer.md)
