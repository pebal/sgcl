[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::close, async_close

```cpp
/*(1)*/ expected<void, error> close() const;
/*(2)*/ async::task<expected<void, error>> async_close() const noexcept;
```

Drops what the block holds and closes the stream underneath, when it has a close (a [file](../file.md), a
connection), as [buffered_writer::close](../buffered_writer/close.md) closes its writer; a stream with none (a
`buffer`) has nothing to close, and the close succeeds. A read after it is the stream's: a closed file answers it
with `errc::closed`.

1. Closes on the calling thread: the stream's `close`, or its `async_close` waited for when it has only that.
2. The same for a task: the stream's `async_close`, or its `close` on the blocking pool when it has only that, as
   [io::reader](../reader.md) closes it.

## Parameters

None.

## Return value

Nothing, or the error of the stream's close, as it gave it.

## Complexity

Constant: the close of the stream.

## Exceptions

- (1) What the close of the stream underneath throws.
- (2) None: what the close throws is the task's, its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("close.txt", "first\nsecond\n");
    io::buffered_reader in(*io::open("close.txt"));
    println("{}", **in.read_line());
    println("{}", bool(in.close()));

    auto after = in.read_line();
    println("{}", after.error().message());
}
```

Output:

```text
first
true
read close.txt: stream closed
```

## See also

- [underlying](underlying.md): the stream it closes
- [buffered_writer::close](../buffered_writer/close.md): the writer's, which flushes first
- [sgcl::io::buffered_reader](../buffered_reader.md)
