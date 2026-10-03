[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::writer::close, async_close

```cpp
/*(1)*/ expected<void, error> close() const;
/*(2)*/ async::task<expected<void, error>> async_close() const noexcept;
```

Closes the stream through the writer, as Go's `io.WriteCloser` does: a file held as an `io::writer` is closed by
it, its descriptor released now rather than by its destructor on the collector's thread, and the error of the close
(a write the system deferred) reported.

1. Calls the stream's `close`; of a stream that has only `async_close`, its task is spawned and waited for here.
2. Returns a task that calls the stream's `async_close`; of a stream that has only `close`, the task runs it on the
   [blocking pool](../../async/spawn_blocking.md).

A stream that has neither, and an empty writer, close nothing and succeed, and (2) touches no pool.

## Parameters

None.

## Return value

Nothing, or the error the stream's close reports.

## Complexity

That of the stream's close; for a half the writer made, a task spawned and waited for (1), or a job on the blocking
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

struct Sink {
    expected<size_t, io::error> write(const slice<const byte>& data) {
        return data.size();
    }

    expected<void, io::error> close() {
        return unexpected(io::error(io::errc::closed, "close", "sink"));
    }
};

async::task<void> close_later(io::writer w) {
    auto done = co_await w.async_close();  // close alone: run on the blocking pool
    println("async_close: {}", done.error().message());
}

int main() {
    Sink sink;
    io::writer w = sink;
    println("close: {}", w.close().error().message());
    async::run(close_later(w));
    println("stdout: {}", io::writer(io::stdout).close().has_value());
}
```

Output:

```text
close: close sink: stream closed
async_close: close sink: stream closed
stdout: true
```

## See also

- [has_close](has_close.md): whether the stream has a close
- [file::close](../file/close.md): the close of a file
- [sgcl::io::writer](../writer.md)
