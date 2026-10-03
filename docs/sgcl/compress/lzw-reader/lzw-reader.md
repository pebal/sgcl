[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw/README.md) › [reader](README.md)

# sgcl::compress::lzw::reader::reader

```cpp
reader(const io::reader& in, order o, int literal_width);
```

Constructs a reader of the LZW codes read from `in`, packed in the bit order `o`, of literals of `literal_width` bits.
Nothing is read yet.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the codes come from |
| `o` | the order of the bits in the bytes ([lzw::order](../lzw-order.md)) |
| `literal_width` | the bits of a literal, 2 to 8 |

## Complexity

Constant; the decoder and its buffers are allocated.

## Exceptions

`std::invalid_argument` when `literal_width` is outside 2 to 8: a mistake of the program.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> pixels = {byte(0), byte(1), byte(1), byte(1), byte(2), byte(1), byte(1), byte(1)};
    auto packed = compress::lzw::compress(pixels, compress::lzw::order::lsb, 2);  // GIF's codes
    compress::lzw::reader r{io::buffer(packed), compress::lzw::order::lsb, 2};
    auto all = r.read_all();
    for (byte b : *all) {
        print("{}", int(b));
    }
    println();
}
```

Output:

```text
01112111
```

## See also

- [read](read.md)
- [sgcl::compress::lzw::reader](README.md)
