[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::close, async_close

```cpp
/*(1)*/ expected<void, error> close();
/*(2)*/ async::task<expected<void, error>> async_close() noexcept;
```

Ends the archive: ends the current entry and writes the central directory, with the ZIP64 records when there are
65 535 entries or more or an offset past 4 GiB, and the end record with the comment. `out` stays open; the program
closes it itself. The result is the kept error, of this close and of every one after it: a stream written freely
is checked once, here.

1. Blocks the calling thread for the writes of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes.

## Parameters

None.

## Return value

Nothing, or the writer's first [error](../error.md), of any step before.

## Complexity

Linear in the number of entries.

## Exceptions

- (1) What the `write` of `out` throws.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    (void)w.add("a.txt", "a");
    println("{} {} bytes", w.close().has_value(), archive.size());
    println("{}", w.close().has_value());
}
```

Output:

```text
true 145 bytes
true
```

## See also

- [last_error](last_error.md)
- [sgcl::compress::zip::writer](../zip-writer.md)
