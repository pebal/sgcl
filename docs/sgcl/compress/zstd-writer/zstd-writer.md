[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [writer](README.md)

# sgcl::compress::zstd::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet. Options out of range are
not refused here: the first write reports them as `errc::invalid_argument`.

1. The default options: level 3, the content checksum, the level's window, no dictionary.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer/README.md) |
| `o` | the level, the checksum, the window, a dictionary ([options](../zstd-options.md)) |

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
    compress::zstd::writer w(sink, {.window_log = 40});
    auto wrote = w.write("hello");
    println("{}", wrote.error().message());
}
```

Output:

```text
write zstd: a window_log of 10..31: invalid argument
```

## See also

- [zstd::options](../zstd-options.md)
- [sgcl::compress::zstd::writer](README.md)
