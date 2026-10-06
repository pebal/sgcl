[sgcl](../README.md) › [compress](README.md) › [zstd](zstd/README.md)

# sgcl::compress::zstd::options

```cpp
#include "sgcl/compress/zstd.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zstd {
    public:
        struct options {
            zstd::level level;
            bool checksum = true;
            bool content_size = true;
            uint8_t window_log = 0;
            zstd::dictionary dictionary;
            bool write_dictionary_id = true;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::zstd::options` is how a zstd frame is made: the [level](zstd-level/README.md), the XXH64 checksum of
the content, the content's size in the header, the window, and a [dictionary](zstd-dictionary/README.md) with whether
the frame names its id. [compress](zstd/compress.md) and the [writer](zstd-writer/README.md) take all of it; the readers
only the dictionary, the rest coming from the frame's header.

## Rules

- An aggregate: `{.level = 19, .checksum = false}`, the other fields at their defaults, which are the `zstd`
  command's.
- **The window** is how far back a match may reach, and what a reader must hold: 2^`window_log` bytes. 0 leaves it to
  the level (2^19 at level 1, 2^21 at 3, 2^23 at 17 to 19, up to 2^27 at 22), and [compress](zstd/compress.md) never
  makes it larger than the data. A larger window finds matches farther back in a long stream; a smaller one bounds a
  reader's memory. A value outside 10 to 31 is the program's mistake: `compress` throws `std::invalid_argument`, and a
  writer's first write reports `errc::invalid_argument`.
- **The content's size** is written by `compress`, which knows it; a writer cannot, whatever the field says.
- **The dictionary** is shared by the options (a handle): the frame is compressed against its content, with its tables
  and offsets when it has them, and names its id unless `write_dictionary_id` is `false` (a frame for a reader that
  knows which dictionary to use anyway saves four bytes).

## Member objects

| Member | Description |
|---|---|
| `level` | the [level](zstd-level/README.md); 3 by default |
| `checksum` | the XXH64 of the content, its low 32 bits at the end of the frame; `true` by default |
| `content_size` | the content's size in the header, where it is known; `true` by default |
| `window_log` | a window of 2^`window_log` bytes, 10 to 31; 0, the level's, by default |
| `dictionary` | the [dictionary](zstd-dictionary/README.md) to compress against and to read with; none by default |
| `write_dictionary_id` | the dictionary's id in the header, when it has one; `true` by default |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string common = "{\"user\": \"\", \"action\": \"login\", \"time\": \"\"}";
    string message = "{\"user\": \"ann\", \"action\": \"login\", \"time\": \"10:30\"}";
    compress::zstd::options o{.dictionary = compress::zstd::dictionary(slice<const byte>(common))};
    println("without: {}", compress::zstd::compress(message).size());
    auto packed = compress::zstd::compress(message, o);
    println("with:    {}", packed.size());
    println("{}", string(slice<const byte>(*compress::zstd::decompress(packed, o))));
}
```

Output:

```text
without: 64
with:    32
{"user": "ann", "action": "login", "time": "10:30"}
```

## See also

- [level](zstd-level/README.md), [dictionary](zstd-dictionary/README.md)
- [sgcl::compress::zstd](zstd/README.md)
