[sgcl](../../README.md) › [io](../README.md) › [multi_reader](README.md)

# sgcl::io::multi_reader::read, async_read

```cpp
expected<size_t, error> read(const slice<byte>& buffer);                         // (1)
async::task<expected<size_t, error>> async_read(slice<byte> buffer) noexcept;    // (2)
```

Reads from the current reader into `buffer`. A reader that ends (a read of 0) is left for the next one, which the
same call reads, so a read returns 0 only after the last reader has ended. A read never spans two readers: it
returns what the current one gave, and a buffer is filled across them by
[read_full](../mixin/reader/read_full.md). An error is returned as it is, and the reader that failed stays the
current one. An empty `buffer` is a read of 0 from the current reader.

1. Reads with each reader's `read`.
2. Returns a task that reads with each reader's `async_read`; a reader that has only `read` has it run on the
   [blocking pool](../../async/spawn_blocking.md) by its handle.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read from one reader, 0 when the last one has ended; or the error of the current reader.

## Complexity

That of the reads, one for each reader that ends during the call, and one more.

## Exceptions

- (1) What a reader's `read` throws.
- (2) None. What a reader's read throws is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> reads(io::multi_reader& parts) {
    vector<byte> block(8);
    while (true) {
        auto n = co_await parts.async_read(block);
        if (!n || *n == 0) {
            break;
        }
        println("{} bytes", *n);
    }
}

int main() {
    io::multi_reader parts({io::buffer("abc"), io::buffer(""), io::buffer("0123456789")});
    async::run(reads(parts));
}
```

Output:

```text
3 bytes
8 bytes
2 bytes
```

## See also

- [read_full](../mixin/reader/read_full.md): a buffer filled across the readers
- [sgcl::io::multi_reader](README.md)
