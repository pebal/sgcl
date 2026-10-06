[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md) › [writer](README.md)

# sgcl::compress::brotli::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet. Options out of range are
not refused here: the first write reports them as `errc::invalid_argument`.

1. The default options: quality 11, a window of 2^22 - 16 bytes.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer/README.md) |
| `o` | the quality and the window ([options](../brotli-options.md)) |

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
    compress::brotli::writer w(sink, {.window_log = 30});
    auto wrote = w.write("hello");
    println("{}", wrote.error().message());
}
```

Output:

```text
write brotli: a window_log of 10..24: invalid argument
```

## See also

- [brotli::options](../brotli-options.md)
- [sgcl::compress::brotli::writer](README.md)
