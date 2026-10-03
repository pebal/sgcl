[sgcl](../README.md) › [compress](README.md) › [xz](xz/README.md)

# sgcl::compress::xz::check

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class xz {
    public:
        enum class check : uint8_t {
            none = 0,
            crc32 = 1,
            crc64 = 4,
            sha256 = 10
        };
    };
}
```

The check of the data of each block of an `.xz` stream, named in the stream's header; the values are the format's
own ids. The writer computes it over the data decompressed and writes it after the block, the reader compares it
after the block's last byte (`errc::checksum`). CRC-64 is xz's default and the [options](xz-options.md)'. CRC-64 runs
on the processor's carry-less multiply ([hash::crc64](../hash/README.md)); SHA-256 is
[crypto](../crypto/README.md)'s.

| Value | Description |
|---|---|
| `none` | no check: the data is only as safe as LZMA2's structure makes it; read and written, as xz does |
| `crc32` | CRC-32, 4 bytes |
| `crc64` | CRC-64, 8 bytes, the default |
| `sha256` | SHA-256, 32 bytes |

The ids the format reserves but does not define are read as `errc::unsupported` (xz warns and goes on).

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    using check = compress::xz::check;
    for (auto c : {check::none, check::crc32, check::crc64, check::sha256}) {
        println("{}", compress::xz::compress(text, {.check = c}).size());
    }
}
```

Output:

```text
84
88
92
116
```

## See also

- [xz::options](xz-options.md)
- [sgcl::compress::xz](xz/README.md)
