[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md) › [writer](../lzw-writer.md)

# sgcl::compress::lzw::writer::writer

```cpp
writer(const io::writer& out, order o, int literal_width);
```

Constructs a writer that compresses what is written to it into `out`, as LZW codes in the bit order `o` of literals
of `literal_width` bits. Nothing is written yet.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the codes go to: any [io writer](../../io/writer.md) |
| `o` | the order of the bits in the bytes ([order](../lzw-order.md)) |
| `literal_width` | the bits of a literal, 2 to 8 |

## Complexity

Constant; the encoder's table is allocated.

## Exceptions

`std::invalid_argument` when `literal_width` is outside 2 to 8: a mistake of the program.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    io::buffer sink;
    try {
        compress::lzw::writer w(sink, compress::lzw::order::lsb, 12);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
compress::lzw: literal width outside 2..8
```

## See also

- [write](write.md)
- [sgcl::compress::lzw::writer](../lzw-writer.md)
