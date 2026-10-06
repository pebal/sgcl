[sgcl](../README.md) › [compress](README.md) › [lz4](lz4/README.md)

# sgcl::compress::lz4::options

```cpp
#include "sgcl/compress/lz4.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lz4 {
    public:
        struct options {
            lz4::level level;
            lz4::block_size block_size = lz4::block_size::mb4;
            bool linked_blocks = false;
            bool block_checksum = false;
            bool content_checksum = true;
            slice<const byte> dictionary;
            uint32_t dictionary_id = 0;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::lz4::options` is how an LZ4 frame is made: the [level](lz4-level/README.md), the largest block
([block_size](lz4-block_size.md)), whether a block may refer to the 64 KB before it, the XXH32 checksums of every block
and of the content, and a dictionary with its id. [compress](lz4/compress.md) and the [writer](lz4-writer/README.md) take
all of it; [compress_block](lz4/compress_block.md) the level and the dictionary; the readers only the dictionary and its
id, the rest coming from the frame's header.

## Rules

- An aggregate: `{.level = 9, .linked_blocks = true}`, the other fields at their defaults, which are the `lz4`
  command's.
- **Linked blocks** compress better with small blocks (a block of 64 KB finds its matches in the one before), and a
  reader must then decode from the frame's start; independent blocks can be decoded alone and are the default.
- **The dictionary** is held by the slice while the options live; the compressor and the decoder copy its last 64 KB.
  Its id is the application's own number for it: written in the header when not 0, and on reading compared with the
  frame's when both are given; a frame that names one cannot be read without a dictionary.
- A block size that is not one of the four is the program's mistake: `compress` throws `std::invalid_argument`, and a
  writer's first write reports `errc::invalid_argument`.

## Member objects

| Member | Description |
|---|---|
| `level` | the [level](lz4-level/README.md); 1, the fast compressor, by default |
| `block_size` | the largest block of the frame; `block_size::mb4` by default |
| `linked_blocks` | a block may refer to the 64 KB before it (`lz4 -BD`); `false` by default |
| `block_checksum` | the XXH32 of every block's bytes (`lz4 -BX`); `false` by default |
| `content_checksum` | the XXH32 of the content, at the end of the frame; `true` by default |
| `dictionary` | bytes to compress against and to read with (their last 64 KB); none by default |
| `dictionary_id` | the dictionary's id in the header, when not 0; 0 by default |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string common = "{\"user\": \"\", \"action\": \"login\", \"time\": \"\"}";
    string message = "{\"user\": \"ann\", \"action\": \"login\", \"time\": \"10:30\"}";
    compress::lz4::options o{.dictionary = slice<const byte>(common), .dictionary_id = 1};
    println("without: {}", compress::lz4::compress(message).size());
    auto packed = compress::lz4::compress(message, o);
    println("with:    {}", packed.size());
    println("{}", string(slice<const byte>(*compress::lz4::decompress(packed, o))));
}
```

Output:

```text
without: 78
with:    51
{"user": "ann", "action": "login", "time": "10:30"}
```

## See also

- [level](lz4-level/README.md), [block_size](lz4-block_size.md)
- [sgcl::compress::lz4](lz4/README.md)
