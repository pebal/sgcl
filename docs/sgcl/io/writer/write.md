[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::writer::write, async_write

```cpp
expected<size_t, error> write(const slice<const byte>& data) const;                // (1)
async::task<expected<size_t, error>> async_write(const slice<const byte>& data)    // (2)
    const noexcept;
```

Writes `data` to the stream: all of it, or an error that says how far it got, as Go's `Write`. Both are `const`, as
a call through a pointer is: the writer is not what a write changes.

1. Writes on the calling thread. For a stream that has only `async_write`, its task is spawned and waited for here.
2. Returns a task that writes. For a stream that has only `write`, the task runs it on the
   [blocking pool](../../async/spawn_blocking.md); `data` without an owner is first copied into a managed block, while
   the caller waits.

The writer brings in the overloads of [mixin::writer](../mixin/writer/write.md) as well, which write a text (a
string, a text slice, a literal, a C string, a `std::string_view`) or one byte through these. The writer may not be
empty: a debug build asserts.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write; a `vector<byte>`, an `array` or another slice converts to it |

## Return value

The number of bytes written, `data.size()`, or the stream's error.

## Complexity

That of the stream's write; for a half the writer made, a task spawned and waited for (1), or a job on the blocking
pool (2).

## Exceptions

- (1) What the stream's own `write` throws; for a stream that has only `async_write`, what its task throws,
  rethrown by the wait.
- (2) None: what the task throws is rethrown by the `co_await`. A stream's own `async_write` that throws before it
  returns its task, which a coroutine cannot, ends the program.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> report(io::writer out, vector<byte> data) {
    auto n = co_await out.async_write(data);
    println(" ({} bytes from a task)", *n);
}

int main() {
    io::writer out = io::stdout;
    vector<byte> data = {byte('o'), byte('k')};
    auto n = out.write(data);
    println(" ({} bytes)", *n);

    async::run(report(out, data));

    io::writer blocking = [](slice<const byte> d) { print("{} bytes on the pool\n", d.size()); };
    async::run(report(blocking, data));
}
```

Output:

```text
ok (2 bytes)
ok (2 bytes from a task)
2 bytes on the pool
 (2 bytes from a task)
```

## See also

- [mixin::writer::write](../mixin/writer/write.md): a text or one byte
- [has_write](has_write.md), [has_async_write](has_async_write.md): which half is the stream's own
- [sgcl::io::writer](../writer.md)
