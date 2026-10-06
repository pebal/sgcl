[sgcl](../../README.md) › [compress](../README.md) › [lz4](../lz4/README.md) › [reader](README.md)

# sgcl::compress::lz4::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;                              // (1)
reader(const io::reader& in, const options& o) noexcept;                     // (2)
reader(const io::reader& in, const limits& l) noexcept;                      // (3)
reader(const io::reader& in, const options& o, const limits& l) noexcept;    // (4)
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header of the stream.

1. The default options and limits.
2. With the dictionary of the options (the only part a reader reads) and its id, checked against the frame's when both are given.
3. With the limits given: their `max_memory` bounds the block size a frame may ask for; `max_size` is not read (a stream has no bound on its output).
4. Both.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |
| `o` | the dictionary and its id ([options](../lz4-options.md)) |
| `l` | `max_memory`, the bound on a frame's block size ([limits](../limits.md)) |

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
    auto packed = compress::lz4::compress(string("x").repeat(1000));  // blocks of 4 MB
    compress::lz4::reader r{io::buffer(packed), compress::limits{.max_memory = 1 << 20}};
    auto all = r.read_all();
    println("{}", all.has_value() ? "read" : r.last_error()->message());
}
```

Output:

```text
offset 15: lz4: the frame's block size needs more memory than the limit allows
```

## See also

- [read](read.md)
- [sgcl::compress::lz4::reader](README.md)
