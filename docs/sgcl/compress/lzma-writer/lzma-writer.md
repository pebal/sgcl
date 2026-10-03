[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md) › [writer](../lzma-writer.md)

# sgcl::compress::lzma::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet, and nothing large is
allocated: the window and the tables are taken at the first write, as large as the level's dictionary. Options out
of range are not refused here: the first write reports them as `errc::invalid_argument`.

1. The default options: level 6.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer.md) |
| `o` | the level, the dictionary, the literal and position bits ([options](../lzma-options.md)) |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::lzma::writer w(sink, {.dictionary = 1000});  // under 4 KiB
    auto wrote = w.write("hello");
    println("{}", wrote.error().message());
}
```

Output:

```text
write lzma: invalid argument
```

## See also

- [lzma::options](../lzma-options.md)
- [sgcl::compress::lzma::writer](../lzma-writer.md)
