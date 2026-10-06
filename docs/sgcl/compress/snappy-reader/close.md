[sgcl](../../README.md) › [compress](../README.md) › [snappy](../snappy/README.md) › [reader](README.md)

# sgcl::compress::snappy::reader::close, async_close

```cpp
expected<void, io::error> close();                                // (1)
async::task<expected<void, io::error>> async_close() noexcept;    // (2)
```

Closes `in`, as `io::buffered_reader`'s close does: a reader made over a file it opened is closed with it. Whatever
was not read is dropped with the stream.

1. Blocks the calling thread for the close of `in`.
2. Returns the task of `in`'s close.

## Parameters

None.

## Return value

Nothing, or the error of `in`'s close.

## Complexity

What the close of `in` costs.

## Exceptions

- (1) What the `close` of `in` throws.
- (2) None. What the close of `in` throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer data(compress::snappy::compress("hello"));
    compress::snappy::reader r(data);
    println("{}", r.read_all_text().value_or(string()));
    println("{}", r.close().has_value());
}
```

Output:

```text
hello
true
```

## See also

- [sgcl::compress::snappy::reader](README.md)
