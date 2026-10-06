[sgcl](../../README.md) › [compress](../README.md) › [lz4](../lz4/README.md) › [writer](README.md)

# sgcl::compress::lz4::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet. Options out of range are
not refused here: the first write reports them as `errc::invalid_argument`.

1. The default options: level 1, blocks of 4 MB, independent, the content checksum.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer/README.md) |
| `o` | the level, the block size and kind, the checksums, a dictionary ([options](../lz4-options.md)) |

## Complexity

Constant; the writer's buffers and tables are allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::lz4::writer w(sink, {.block_size = compress::lz4::block_size(9)});
    auto wrote = w.write("hello");
    println("{}", wrote.error().message());
}
```

Output:

```text
write lz4: a block size of kb64, kb256, mb1 or mb4: invalid argument
```

## See also

- [lz4::options](../lz4-options.md)
- [sgcl::compress::lz4::writer](README.md)
