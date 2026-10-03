[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md)

# sgcl::compress::lzw::decompress

```cpp
/*(1)*/ static expected<vector<byte>, error> decompress(const slice<const byte>& data, order o,
                                                        int literal_width);
/*(2)*/ static expected<vector<byte>, error> decompress(const slice<const byte>& data, order o,
                                                        int literal_width,
                                                        const limits& l);
```

Decompresses the whole of the LZW codes at once, in the order `o`, of literals of `literal_width` bits. It stops
at the end code and reads nothing after it; a table that fills with no clear stays full until a clear comes, as Go
reads GIF's deferred clear. The output stops at the limits' `max_size` with `errc::too_large`.

1. With the default [limits](../limits.md): 1 GiB of output.
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the codes |
| `o` | the order of the bits in the bytes ([order](../lzw-order.md)) |
| `literal_width` | the bits of a literal, 2 to 8 |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error.md): a code that is not one yet (`errc::corrupt`), data that ends
before the end code (`errc::unexpected_end`), output past `max_size` (`errc::too_large`).

## Complexity

Linear in the size of the output.

## Exceptions

`std::invalid_argument` when `literal_width` is outside 2 to 8: a mistake of the program.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> pixels(1000, byte(7));
    auto packed = compress::lzw::compress(pixels, compress::lzw::order::lsb, 8);
    println("{}", compress::lzw::decompress(packed, compress::lzw::order::lsb, 8)->size());

    auto small = compress::lzw::decompress(packed, compress::lzw::order::lsb, 8, {.max_size = 100});
    println("{}", small.error().message());
    packed.pop_back();
    auto cut = compress::lzw::decompress(packed, compress::lzw::order::lsb, 8);
    println("{}", cut.error().message());
}
```

Output:

```text
1000
offset 16: lzw: decompressed data past the limit
offset 52: lzw: unexpected end of the data (no end code)
```

## See also

- [compress](compress.md): the other way
- [lzw::reader](../lzw-reader.md): a stream
- [sgcl::compress::lzw](../lzw.md)
