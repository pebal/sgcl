[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [reader](README.md)

# sgcl::compress::zstd::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;                              // (1)
reader(const io::reader& in, const options& o) noexcept;                     // (2)
reader(const io::reader& in, const limits& l) noexcept;                      // (3)
reader(const io::reader& in, const options& o, const limits& l) noexcept;    // (4)
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header of the stream.

1. The default options and limits.
2. With the dictionary of the options (the only part a reader reads), checked against the id a frame names.
3. With the limits given: their `max_memory` bounds the window a frame may ask for; `max_size` is not read (a stream has no bound on its output).
4. Both.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `o` | the dictionary ([options](../zstd-options.md)) |
| `l` | `max_memory`, the bound on a frame's window ([limits](../limits.md)) |

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
    compress::zstd::writer w(packed, {.level = 19});  // a window of 8 MB in the header
    w.write(string("x").repeat(1000));
    (void)w.close();
    compress::zstd::reader r{packed, compress::limits{.max_memory = 1 << 20}};
    auto all = r.read_all();
    println("{}", all.has_value() ? "read" : r.last_error()->message());
}
```

Output:

```text
offset 6: zstd: the frame's window needs more memory than the limit allows
```

## See also

- [read](read.md)
- [sgcl::compress::zstd::reader](README.md)
