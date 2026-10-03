[sgcl](../README.md) › [compress](README.md) › [xz](xz/README.md)

# sgcl::compress::xz::options

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class xz {
    public:
        struct options {
            compress::level level;
            bool extreme = false;
            xz::check check = xz::check::crc64;
            optional<xz::filter> bcj;
            uint16_t delta = 0;
            uint32_t dictionary = 0;
        };
    };
}
```

`sgcl::compress::xz::options` is how an `.xz` stream is made: the level and dictionary of [lzma](lzma-options.md)
(xz's `-0` to `-9` and `-e`), the [check](xz-check.md) of each block, CRC-64 unless told, and the filters before
LZMA2: a branch converter for the named processor ([filter](xz-filter.md)), Delta over bytes `delta` apart, or both,
the converter first. [compress](xz/compress.md) and the [writer](xz-writer/README.md) take it; the decoders read all of it
from the headers.

## Rules

- An aggregate: `{.level = 9, .bcj = compress::xz::filter::arm64}`, the other fields at their defaults.
- A converter fits the code of one processor: an ARM64 program of 32 MB compresses 12 % smaller with the ARM64
  converter; a universal macOS binary 3 % with either converter, each fitting half of it.
- Out of range — a Delta distance past 256, a check that is not one of the four, a dictionary under 4 KiB or past
  1.5 GiB, `level::huffman_only` — is the program's mistake: `compress` throws `std::invalid_argument`, and a
  writer's first write reports `errc::invalid_argument`.

## Member objects

| Member | Description |
|---|---|
| `level` | 0 to 9 as xz `-0` to `-9`; 6 by default ([lzma::options](lzma-options.md) has the table) |
| `extreme` | the deeper search of xz's `-e`; `false` by default |
| `check` | the check of each block; `check::crc64` by default, xz's |
| `bcj` | a branch converter before LZMA2; none by default |
| `delta` | Delta's distance, 1 to 256, before LZMA2 (after the converter); 0, by default, none |
| `dictionary` | the dictionary in bytes, 4 KiB to 1.5 GiB; 0, by default, is the level's |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // 16-bit samples of a slow wave: each close to the one two bytes before
    vector<byte> sound;
    for (int i : range(20000)) {
        int sample = (i % 400 < 200 ? i % 200 : 200 - i % 200) * 100;
        sound.push_back(byte(sample & 0xff));
        sound.push_back(byte(sample >> 8));
    }
    println("plain:   {}", compress::xz::compress(sound).size());
    println("delta 2: {}", compress::xz::compress(sound, {.delta = 2}).size());
}
```

Output:

```text
plain:   872
delta 2: 212
```

## See also

- [check](xz-check.md), [filter](xz-filter.md)
- [lzma::options](lzma-options.md): the levels
- [sgcl::compress::xz](xz/README.md)
