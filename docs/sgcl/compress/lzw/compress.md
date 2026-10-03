[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md)

# sgcl::compress::lzw::compress

```cpp
static vector<byte> compress(const slice<const byte>& data, order o, int literal_width);
```

Compresses the whole of the data at once into LZW codes, in the order `o`, of literals of `literal_width` bits:
a clear code first, the codes, the end code. When the table fills, a clear code goes in place of its last code, as
Go does: the output is Go's, byte for byte.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to compress, each below 2^`literal_width` |
| `o` | the order of the bits in the bytes ([order](../lzw-order.md)) |
| `literal_width` | the bits of a literal, 2 to 8 |

## Return value

The compressed bytes, a new vector.

## Complexity

Linear in the size of the data.

## Exceptions

`std::invalid_argument` when `literal_width` is outside 2 to 8, or a byte of `data` does not fit it (5 with a width
of 2): mistakes of the program.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    vector<byte> pixels = {byte(0), byte(1), byte(2), byte(3), byte(0), byte(1), byte(2), byte(3)};
    println("{}", compress::lzw::compress(pixels, compress::lzw::order::lsb, 2).size());
    try {
        pixels.push_back(byte(5));
        compress::lzw::compress(pixels, compress::lzw::order::lsb, 2);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
4
compress::lzw: a byte past the literal width
```

## See also

- [decompress](decompress.md): the other way
- [lzw::writer](../lzw-writer.md): a stream
- [sgcl::compress::lzw](../lzw.md)
