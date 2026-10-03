[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [writer](../tar-writer.md)

# sgcl::compress::tar::writer::close, async_close

```cpp
/*(1)*/ expected<void, io::error> close();
/*(2)*/ async::task<expected<void, io::error>> async_close() noexcept;
```

Ends the archive: pads the last entry's data to its block and writes two blocks of zeros, and leaves `out` open; the
program closes `out` itself (a gzip writer under it, then its file). A close before the last entry's data is whole is
`errc::invalid_argument`. A second close after a close that succeeded does nothing; after an error kept, every close
gives it and writes nothing.

1. Blocks the calling thread for the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes.

## Parameters

None.

## Return value

Nothing, or the writer's first error: the last entry's data not whole, a failure of `out`, or any error kept from
before.

## Complexity

Constant.

## Exceptions

- (1) What the `write` of `out` throws; the errors of the stream and of the archive are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    (void)w.write_header({.name = "a.txt", .size = 3});
    (void)w.write("ab");
    println("{}", w.close().error().message());  // one byte short
}
```

Output:

```text
close tar entry a.txt: invalid argument
```

## See also

- [last_error](last_error.md)
- [sgcl::compress::tar::writer](../tar-writer.md)
