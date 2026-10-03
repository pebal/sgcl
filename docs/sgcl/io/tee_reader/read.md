[sgcl](../../README.md) › [io](../README.md) › [tee_reader](../tee_reader.md)

# sgcl::io::tee_reader::read, async_read

```cpp
/*(1)*/ expected<size_t, error> read(const slice<byte>& buffer);
/*(2)*/ async::task<expected<size_t, error>> async_read(slice<byte> buffer) noexcept;
```

Reads from the source into `buffer`, then writes the bytes read to the writer, before the read returns. An error of
the write is the read's error: the bytes are in `buffer`, but the read reports the write's failure.

1. Reads with the source's `read` and writes with the writer's `write`.
2. Returns a task that reads with the source's `async_read` and writes with the writer's `async_write`; a stream
   that has only the blocking half has it run on the [blocking pool](../../async/spawn_blocking.md) by its handle.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read, 0 at the end of the source; or the error of the read or of the write.

## Complexity

That of the source's read and the writer's write.

## Exceptions

- (1) What the source's `read` or the writer's `write` throws.
- (2) None. What they throw is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> sum(io::reader source) {
    size_t mirrored = 0;
    io::writer counter = [&](slice<const byte> data) { mirrored += data.size(); };
    io::tee_reader input(source, counter);
    vector<byte> block(16);
    auto n = co_await input.async_read(block);
    println("{} read, {} mirrored", *n, mirrored);

    io::writer broken = [](slice<const byte>) -> expected<size_t, io::error> {
        return unexpected(io::error(io::errc::closed, "write", "mirror"));
    };
    io::tee_reader failing(source, broken);
    auto r = failing.read(block);
    println("{}", r.error().message());
}

int main() {
    async::run(sum(io::buffer("twenty bytes of text")));
}
```

Output:

```text
16 read, 16 mirrored
write mirror: stream closed
```

## See also

- [sgcl::io::tee_reader](../tee_reader.md)
