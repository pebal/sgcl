[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md)

# sgcl::compress::lzma::decompress

```cpp
/*(1)*/ static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;
/*(2)*/ static expected<vector<byte>, error> decompress(const slice<const byte>& data,
                                                        const limits& l) noexcept;
```

Decompresses the whole of an `.lzma` stream at once, the size in the header or the end marker after the data, or
both. The header is checked against the limits first: the dictionary it asks for (as large as the data when the
header gives its size) against `max_memory`, the size it gives against `max_size`, so data past them fails before any
work; an output that grows past `max_size` stops there. Nothing after the LZMA data is read.

1. With the default [limits](../limits.md): 1 GiB of output, a dictionary of up to 1 GiB.
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bounds on the dictionary and the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error.md): a properties byte past 224 (`errc::invalid_header`), data cut
short (`errc::unexpected_end`), data the format does not allow (`errc::corrupt`), a dictionary or an output past the
limits (`errc::too_large`).

## Complexity

Linear in the size of the output.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::lzma::compress(string("x").repeat(1 << 20));
    println("{}", compress::lzma::decompress(packed)->size());
    println("{}", compress::lzma::decompress(packed, {.max_size = 1000}).error().message());
    auto cut = slice<const byte>(packed).first(10);
    println("{}", compress::lzma::decompress(cut).error().message());
}
```

Output:

```text
1048576
offset 0: lzma: decompressed data past the limit
offset 10: lzma: unexpected end in the header
```

## See also

- [compress](compress.md): the other way
- [lzma::reader](../lzma-reader.md): a stream
- [limits](../limits.md)
- [sgcl::compress::lzma](../lzma.md)
