[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [reader](README.md)

# sgcl::compress::tar::reader::close, async_close

```cpp
expected<void, io::error> close();                                // (1)
async::task<expected<void, io::error>> async_close() noexcept;    // (2)
```

Closes `in`, as `io::buffered_reader`'s close does: a reader made over a file, or over a decompressing reader of a
file, closes it. Whatever was not read is dropped.

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
    {
        io::writer file = *io::create("one.tar");
        compress::tar::writer w(file);
        (void)w.write_header({.name = "a.txt", .size = 1});
        (void)w.write("a");
        (void)w.close();
        (void)file.close();
    }
    io::reader file = *io::open("one.tar");
    compress::tar::reader r(file);
    println("{}", (*r.next())->name);
    println("{}", r.close().has_value());
    (void)io::remove("one.tar");
}
```

Output:

```text
a.txt
true
```

## See also

- [sgcl::compress::tar::reader](README.md)
