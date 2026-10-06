[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md)

# sgcl::compress::zstd::dictionary

```cpp
#include "sgcl/compress/zstd.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zstd {
    public:
        class dictionary;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::zstd::dictionary` is a zstd dictionary, read once and given to any number of compressions,
decompressions and streams through the [options](../zstd-options.md). Small messages of one kind — the records of a
log, the bodies of an API, the rows of a table — compress poorly alone, having no history; a dictionary is history
given in advance, both sides holding it. Two kinds are read: zstd's format (what `zstd --train` writes: the magic
0xEC30A437, an id, the Huffman table of the literals, the three FSE tables of the sequences, three repeated offsets,
then the content), whose tables also spare each frame its own; and any other bytes, taken as raw content with id 0.
Training a dictionary from samples is not part of this class: one trained by the `zstd` command is read.

## Rules

- A handle: a copy shares the dictionary, which nothing changes once it is read, so any number of threads may
  compress and decompress with it at once.
- [parse](parse.md) reads bytes and reports a dictionary of zstd's format that is damaged; the constructor from bytes
  throws instead, for bytes the program trusts (DESIGN 234). Bytes that do not begin with the magic are never
  refused: they are raw content.
- The default dictionary is none: options holding it compress without a dictionary.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zstd-dictionary.md) | none, or a dictionary read from bytes (throwing) |
| [parse](parse.md) | a dictionary read from bytes, or the error (static) |

#### Observers

| Function | Description |
|---|---|
| [id](id.md) | the id the dictionary names itself by; 0 for raw content |
| [size](size.md) | the bytes of its content |
| [empty](empty.md) | checks whether it is none |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::zstd::dictionary words(slice<const byte>(string("level=info msg=\"request served\" status=200")));
    compress::zstd::options o{.dictionary = words};
    for (string line : {string("level=info msg=\"request served\" status=200 path=/a"),
                        string("level=info msg=\"request served\" status=404 path=/b")}) {
        println("{} bytes: {} alone, {} with the dictionary", line.size(),
                compress::zstd::compress(line).size(), compress::zstd::compress(line, o).size());
    }
}
```

Output:

```text
50 bytes: 63 alone, 27 with the dictionary
50 bytes: 63 alone, 30 with the dictionary
```

## See also

- [options](../zstd-options.md): where it is given
- [dictionary_id](../zstd/dictionary_id.md): the id a frame names
- [sgcl::compress::zstd](../zstd/README.md)
