[sgcl](../../README.md) › [io](../README.md) › [reader](../reader.md)

# sgcl::io::reader::close, async_close

```cpp
expected<void, error> close() const;                                // (1)
async::task<expected<void, error>> async_close() const noexcept;    // (2)
```

Closes the stream through the reader, as Go's `io.ReadCloser` does: a file held as an `io::reader` is closed by it,
its descriptor released now rather than by its destructor on the collector's thread.

1. Calls the stream's `close`; of a stream that has only `async_close`, its task is spawned and waited for here.
2. Returns a task that calls the stream's `async_close`; of a stream that has only `close`, the task runs it on the
   [blocking pool](../../async/spawn_blocking.md).

A stream that has neither, and an empty reader, close nothing and succeed, and (2) touches no pool.

## Parameters

None.

## Return value

Nothing, or the error the stream's close reports.

## Complexity

That of the stream's close; for a half the reader made, a task spawned and waited for (1), or a job on the blocking
pool (2).

## Exceptions

- (1) What the stream's own `close` throws; for a stream that has only `async_close`, what its task throws,
  rethrown by the wait.
- (2) None: what the task throws is rethrown by the `co_await`. A stream's own `async_close` that throws before it
  returns its task, which a coroutine cannot, ends the program.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Source {
    expected<size_t, io::error> read(const slice<byte>&) {
        return 0;
    }

    expected<void, io::error> close() {
        println("closed");
        return {};
    }
};

async::task<void> close_later(io::reader r) {
    auto done = co_await r.async_close();  // close alone: run on the blocking pool
    println("async_close: {}", done.has_value());
}

int main() {
    Source source;
    io::reader r = source;
    println("close: {}", r.close().has_value());
    async::run(close_later(r));

    io::reader b = io::buffer("no close of its own");
    println("buffer: {}", b.close().has_value());
}
```

Output:

```text
closed
close: true
closed
async_close: true
buffer: true
```

## See also

- [has_close](has_close.md): whether the stream has a close
- [file::close](../file/close.md): the close of a file
- [sgcl::io::reader](../reader.md)
