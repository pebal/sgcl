[sgcl](../../README.md) › [io](../README.md) › [limit_reader](README.md)

# sgcl::io::limit_reader::limit_reader

```cpp
limit_reader(const io::reader& r, uint64_t n) noexcept;
```

Constructs a reader of the first `n` bytes of `r`. The source is held as an [io::reader](../reader/README.md): any stream
converts to one, a handle by its object, a stream of the program's by reference. Nothing is read until the first
read.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the source |
| `n` | the number of bytes to let through; 0 is a reader at its end at once |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer source("0123456789");
    io::limit_reader first(source, 4);
    io::limit_reader none(source, 0);
    println("{} [{}]", *first.read_all_text(), *none.read_all_text());
    println("{}", source.text());
}
```

Output:

```text
0123 []
456789
```

## See also

- [read, async_read](read.md)
- [sgcl::io::limit_reader](README.md)
