[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::flush, async_flush

```cpp
expected<void, error> flush() const;                                // (1)
async::task<expected<void, error>> async_flush() const noexcept;    // (2)
```

Writes what the block holds to the stream underneath, in one write, and empties the block; an empty block writes
nothing. It is Go's `bufio.Writer.Flush`. After an error the writer has kept, a flush gives that error at once and
writes nothing. The destructor does not flush: a writer whose bytes must arrive is flushed or [closed](close.md).

1. Flushes on the calling thread.
2. The same for a task: the block is moved into managed memory by the first async operation, once for the writer's
   life, and written with the stream's `async_write`, or its `write` on the blocking pool for a stream that has only
   that. The writer is held by the handle the task was made from: the caller keeps it alive until the task is done.

## Parameters

None.

## Return value

Nothing, or the error: the one the writer kept, or the error of the stream's write, which it keeps.

## Complexity

Linear in the number of bytes buffered: one write of the stream.

## Exceptions

- (1) What the write of the stream underneath throws: a [file](../file/write.md)'s `std::system_error` when the
  reactor's thread cannot be started.
- (2) None. What the write throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    io::buffered_writer w(out);
    w.write("one, ");
    w.flush();
    println("[{}] {}", out.text(), w.buffered());

    w.write("two");
    w.async_flush().wait();
    println("[{}]", out.text());
}
```

Output:

```text
[one, ] 0
[one, two]
```

## See also

- [close](close.md): flushes, then closes the stream underneath
- [buffered](buffered.md): what a flush would write
- [sgcl::io::buffered_writer](../buffered_writer.md)
