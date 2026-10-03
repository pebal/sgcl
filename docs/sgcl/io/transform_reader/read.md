[sgcl](../../README.md) › [io](../README.md) › [transform_reader](../transform_reader.md)

# sgcl::io::transform_reader\<F\>::read, async_read

```cpp
expected<size_t, error> read(const slice<byte>& buffer);                         // (1)
async::task<expected<size_t, error>> async_read(slice<byte> buffer) noexcept;    // (2)
```

Reads from the source into `buffer`, then calls the function with the bytes just read, `buffer.first(n)`, which
changes them in place before the read returns. A read of 0 and an error do not call it.

1. Reads with the source's `read`.
2. Returns a task that reads with the source's `async_read`, and calls the function in the task when the read is
   done; for a source that has only `read`, the source's `io::reader` runs it on the
   [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go |

## Return value

The number of bytes read, 0 at the end of the source; or the source's error.

## Complexity

That of the source's read, and the function's call over the bytes read.

## Exceptions

- (1) What the source's `read` or the function throws.
- (2) None. What they throw is the task's, rethrown by the `co_await`.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> masked(io::reader source) {
    io::transform_reader hidden(source, [](slice<byte> bytes) {
        for (byte& b : bytes) {
            b = byte('*');
        }
    });
    vector<byte> block(6);
    auto n = co_await hidden.async_read(block);
    println("{} bytes: {}{}{}", *n, char(block[0]), char(block[1]), char(block[5]));
}

int main() {
    async::run(masked(io::buffer("secret password")));
}
```

Output:

```text
6 bytes: ***
```

## See also

- [sgcl::io::transform_reader\<F\>](../transform_reader.md)
