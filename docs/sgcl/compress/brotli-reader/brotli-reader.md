[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md) › [reader](README.md)

# sgcl::compress::brotli::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;            // (1)
reader(const io::reader& in, const limits& l) noexcept;    // (2)
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header of the stream.

1. The default limits.
2. With the limits given: their `max_memory` bounds the window a stream may ask for; `max_size` is not read (a stream has no bound on its output).

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `l` | `max_memory`, the bound on a stream's window ([limits](../limits.md)) |

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
    io::buffer packed;
    compress::brotli::writer w(packed, {.level = 1, .window_log = 24});  // a window of 16 MB in the header
    w.write(string("x").repeat(1000));
    (void)w.close();
    compress::brotli::reader r{packed, compress::limits{.max_memory = 1 << 20}};
    auto all = r.read_all();
    println("{}", all.has_value() ? "read" : r.last_error()->message());
}
```

Output:

```text
offset 0: brotli: the window needs more memory than the limit allows
```

## See also

- [read](read.md)
- [sgcl::compress::brotli::reader](README.md)
