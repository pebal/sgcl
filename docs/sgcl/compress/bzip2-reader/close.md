[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [reader](README.md)

# sgcl::compress::bzip2::reader::close, async_close

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = encoding::hex::decode("425a68393141592653591931653d00000081000244a000"
                                        "219a68334d07338bb9229c28480c98b29e80");
    compress::bzip2::reader r{io::buffer(*packed)};
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

- [sgcl::compress::bzip2::reader](README.md)
