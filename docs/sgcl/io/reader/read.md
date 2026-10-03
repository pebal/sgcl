[sgcl](../../README.md) › [io](../README.md) › [reader](../reader.md)

# sgcl::io::reader::read, async_read

```cpp
/*(1)*/ expected<size_t, error> read(const slice<byte>& buffer) const;
/*(2)*/ async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const noexcept;
```

Reads bytes of the stream into `buffer`, at most `buffer.size()`. Both are `const`, as a call through a pointer is:
the reader is not what a read changes.

1. Reads on the calling thread. For a stream that has only `async_read`, its task is spawned and waited for here.
2. Returns a task that reads. For a stream that has only `read`, the task runs it on the
   [blocking pool](../../async/spawn_blocking.md), a thread of which it holds until the read returns; a `buffer` without
   an owner is read into a managed block, copied into `buffer` when the task resumes.

The reader may not be empty: a debug build asserts.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; a `vector<byte>`, an `array` or another slice converts to it |

## Return value

The number of bytes read, from 1 to `buffer.size()`; 0 at the end of the stream, which is not an error. Or the
stream's error.

## Complexity

That of the stream's read; for a half the reader made, a task spawned and waited for (1), or a job on the blocking
pool (2).

## Exceptions

- (1) What the stream's own `read` throws; for a stream that has only `async_read`, what its task throws, rethrown
  by the wait.
- (2) None: what the task throws is rethrown by the `co_await`. A stream's own `async_read` that throws before it
  returns its task, which a coroutine cannot, ends the program.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<size_t> total(io::reader in) {
    vector<byte> block(4);
    size_t sum = 0;
    while (true) {
        auto n = co_await in.async_read(block);
        if (!n || *n == 0) {
            co_return sum;
        }
        sum += *n;
    }
}

int main() {
    io::reader in = io::buffer("abcdefghij");
    vector<byte> block(4);
    auto n = in.read(block);
    println("{} bytes, the first '{}'", *n, char(block[0]));
    println("{} bytes after them", async::run(total(in)));

    int left = 3;
    // read alone: async_read runs it on the blocking pool
    io::reader blocking = [&](slice<byte> b) -> size_t {
        return left-- > 0 ? b.size() : 0;
    };
    println("{} bytes from the pool", async::run(total(blocking)));
}
```

Output:

```text
4 bytes, the first 'a'
6 bytes after them
12 bytes from the pool
```

## See also

- [read_full](../mixin/reader/read_full.md): fills the whole buffer
- [read_all](../mixin/reader/read_all.md): everything to the end
- [has_read](has_read.md), [has_async_read](has_async_read.md): which half is the stream's own
- [sgcl::io::reader](../reader.md)
