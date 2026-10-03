[sgcl](../README.md) › [io](README.md)

# sgcl::io::discard_writer

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class discard_writer;   // : public mixin::writer<discard_writer>

    inline discard_writer discard;
}
```

`sgcl::io::discard_writer` is the writer that drops everything and reports it written. Its one object is
`io::discard`, a global as `io::stdout` is, written to by reference: `io::copy(io::discard, r)` reads a stream to its
end and counts it, a writer that must be given where nothing is wanted takes it. It is Go's `io.Discard`, and what a
`std::ostream` with no buffer is in the standard library. A write never fails and never waits, in both forms.

## Rules

- The class has no state: `io::discard` lives anywhere, and an object of the class may be made where one is
  needed; every one drops the same.
- Any number of threads and tasks may write to it at once.

## Member functions

| Function | Description |
|---|---|
| [write, async_write](discard_writer/write.md) | takes bytes and drops them |

#### From mixin::writer

The rest of what a writer does, each an algorithm of io over this stream ([mixin::writer](mixin/writer.md)).

| Function | Description |
|---|---|
| [write, async_write](mixin/writer/write.md) | takes a text or one byte and drops it |
| [copy_from, async_copy_from](mixin/writer/copy_from.md) | reads a reader to its end and drops what it read |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer response("HTTP/1.1 200 OK\r\n\r\nthe body nobody needs");
    auto skipped = io::copy(io::discard, response);
    println("{} bytes skipped, {} left", *skipped, response.size());
}
```

Output:

```text
40 bytes skipped, 0 left
```

## See also

- [copy](copy.md): a reader to its end into a writer
- [multi_writer](multi_writer.md): one write to several writers
- [writer](writer.md)
