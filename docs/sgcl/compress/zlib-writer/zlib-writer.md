[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib/README.md) › [writer](README.md)

# sgcl::compress::zlib::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written to `out` yet.

1. The default options: level 6, no dictionary.
2. The options given: the level and a preset dictionary, named by its Adler-32 in the header.

The encoder's memory (its window and tables) is taken here, and kept across a [reset](reset.md).

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer/README.md), a file, a buffer, a socket |
| `o` | the level and a preset dictionary, named by its Adler-32 in the header ([options](../zlib-options.md)) |

## Complexity

Constant; the encoder's tables are allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer fast, small;
    compress::zlib::writer a(fast, {.level = compress::level::fastest});
    compress::zlib::writer b(small, {.level = compress::level::smallest});
    string text = string("the same words, the same words again and again. ").repeat(100);
    a.write(text);
    b.write(text);
    (void)a.close();
    (void)b.close();
    println("{} bytes: {} at level 1, {} at level 9", text.size(), fast.size(), small.size());
}
```

Output:

```text
4800 bytes: 111 at level 1, 71 at level 9
```

## See also

- [close](close.md): the end of the stream
- [sgcl::compress::zlib::writer](README.md)
