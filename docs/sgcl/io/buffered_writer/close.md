[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](README.md)

# sgcl::io::buffered_writer::close, async_close

```cpp
expected<void, error> close() const;                                // (1)
async::task<expected<void, error>> async_close() const noexcept;    // (2)
```

Flushes the block, then closes the stream underneath when it has a close (a [file](../file/README.md), a connection), after
an error too: a writer that failed still gives back its descriptor. The result is the error the writer kept, the
first one it gave — a failure of the stream in a write, the flush or the close itself — so a buffered writer is
written freely and checked once, here. A closed writer refuses every later write with `errc::closed`; a later
`close()` closes nothing again and gives the kept error, or success.

1. Closes on the calling thread: the stream's `close`, or its `async_close` waited for when it has only that.
2. The same for a task: the flush as [async_flush](flush.md), then the stream's `async_close`, or its `close` on the
   blocking pool when it has only that. The writer is held by the handle the task was made from: the caller keeps it
   alive until the task is done.

## Parameters

None.

## Return value

Nothing, or the first error the writer gave, kept: of a write, of the flush, or of the stream's close.

## Complexity

Linear in the number of bytes buffered: the flush, then the close of the stream.

## Exceptions

- (1) What the write and the close of the stream underneath throw: a [file](../file/write.md)'s write
  `std::system_error` when the reactor's thread cannot be started.
- (2) None. What the write or the close throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct row {
    string name;
    int count;
};

int main() {
    vector<row> rows = {{"apples", 3}, {"pears", 5}};
    io::buffered_writer out(*io::create("out.csv"));
    for (auto& r : rows) {
        out.write(r.name);
        out.write(byte(','));
        out.write(to_string(r.count));
        out.write(byte('\n'));
    }
    if (auto closed = out.close(); !closed) {  // the block written, the file closed
        eprintln("{}", closed.error().message());
    }
    print("{}", *io::read_text("out.csv"));
    println("{} {}", out.is_closed(), out.write("late").error().message());
}
```

Output:

```text
apples,3
pears,5
true write: stream closed
```

## See also

- [flush](flush.md): writes the block, the stream left open
- [last_error](last_error.md): the error kept
- [buffered_reader::close](../buffered_reader/close.md): the reader's
- [sgcl::io::buffered_writer](README.md)
