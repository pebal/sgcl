[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz.md) › [reader](../xz-reader.md)

# sgcl::compress::xz::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;            // (1)
reader(const io::reader& in, const limits& l) noexcept;    // (2)
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
stream's header and the first block's.

1. With the default [limits](../limits.md): a block's dictionary of up to 1 GiB.
2. With the limits given: their `max_memory` bounds each block's dictionary; `max_size` is not read (a stream has no
   bound on its output).

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `l` | `max_memory`, the bound on a block's dictionary ([limits](../limits.md)) |

## Complexity

Constant; the input buffer is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::xz::compress(string("x").repeat(2 << 20), {.level = 9});
    compress::xz::reader r{io::buffer(packed), {.max_memory = 1 << 20}};
    auto all = r.read_all();
    println("{}", all.has_value() ? "read" : r.last_error()->message());
}
```

Output:

```text
offset 12: xz: the dictionary needs more memory than the limit allows
```

## See also

- [read](read.md)
- [sgcl::compress::xz::reader](../xz-reader.md)
