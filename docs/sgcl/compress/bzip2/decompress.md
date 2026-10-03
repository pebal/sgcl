[sgcl](../../README.md) › [compress](../README.md) › [bzip2](README.md)

# sgcl::compress::bzip2::decompress

```cpp
static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept;    // (1)
static expected<vector<byte>, error> decompress(const slice<const byte>& data,              // (2)
                                                const limits& l) noexcept;
```

Decompresses the whole of the bzip2 data at once: every stream, one after another, every block's CRC and every
stream's CRC checked. The output stops at the limits' `max_size` with `errc::too_large`: data from outside may
decompress a thousand times over.

1. With the default [limits](../limits.md): 1 GiB of output.
2. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the compressed bytes |
| `l` | the bound on the output ([limits](../limits.md)) |

## Return value

The decompressed bytes, or the [error](../error/README.md): not bzip2 (`errc::invalid_header`), a CRC that does not match
(`errc::checksum`), a block the format does not allow (`errc::corrupt`), a randomised block
(`errc::unsupported`), data cut short (`errc::unexpected_end`), output past `max_size` (`errc::too_large`).

## Complexity

Linear in the size of the output.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // "first part\n" and "second part\n", two streams one after another
    auto two = encoding::hex::decode("425a683931415926535957ec6af0000001d1800010400021205c0020"
                                     "0031064c40d01ea68d15b81c9f8bb9229c28482bf6357800425a6839"
                                     "314159265359f50036de0000055180001040002e01dc002000310340"
                                     "d02000c86bb3481c01f177245385090f50036de0");
    print("{}", string(slice<const byte>(*compress::bzip2::decompress(*two))));
    println("{}", compress::bzip2::decompress(*two, {.max_size = 5}).error().message());
}
```

Output:

```text
first part
second part
offset 42: bzip2: decompressed data past the limit
```

## See also

- [bzip2::reader](../bzip2-reader/README.md): a stream
- [limits](../limits.md)
- [sgcl::compress::bzip2](README.md)
