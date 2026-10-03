[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::flush, async_flush

```cpp
/*(1)*/ expected<void, io::error> flush();
/*(2)*/ async::task<expected<void, io::error>> async_flush() noexcept;
```

Writes what was gathered onto the stream, in one write, and starts gathering again: Go's `Encoder.Flush`. The
stream stays open. A large document is flushed between its parts, so that the writer holds one part at a time.

1. Writes on the thread that calls it.
2. The same in a task: `co_await w.async_flush()` gives the worker back while the stream waits.

Once a mistake is kept ([last_error](last_error.md)), a flush writes nothing — not what was gathered before the
mistake either — and gives the mistake as an `io::error` of the `encoding` category, its code the
[errc](../errc.md) of the mistake. A failure of the stream is kept as well, and every flush after it gives it
again; it is no mistake of the document's, which [last_error](last_error.md) would give, but what is written after
it is dropped as after a mistake, not gathered, so a long-running writer whose stream failed holds no more memory.

## Parameters

None.

## Return value

Nothing; otherwise the error of the stream, or the first mistake made as an `io::error`.

## Complexity

Linear in the length of what was gathered.

## Exceptions

- (1) What the write of the stream throws.
- (2) None: what (1) throws, the task's `co_await` or `wait()` throws again.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> write_in_a_task() {
    encoding::xml::writer w(io::stdout);
    w.start("from").text("a task").end();
    auto done = co_await w.async_flush();
    println("\n{}", done.has_value());
}

int main() {
    encoding::xml::writer w(io::stdout);
    w.start("ok").end();
    println("{}", w.flush().has_value());

    w.start("a").text("t").attribute("late", "1").end();
    auto failed = w.flush();
    println(failed.error().message());

    write_in_a_task().wait();
}
```

Output:

```text
<ok/>true
encode xml: syntax error
<from>a task</from>
true
```

## See also

- [last_error](last_error.md): the first mistake, whole
- [sgcl::encoding::xml::writer](../xml-writer.md)
