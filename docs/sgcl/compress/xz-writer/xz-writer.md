[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz.md) › [writer](../xz-writer.md)

# sgcl::compress::xz::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet, and nothing large is
allocated: the window and the tables are taken at the first write, as large as the level's dictionary. Options out
of range are not refused here: the first write reports them as `errc::invalid_argument`.

1. The default options: level 6, CRC-64, no filter.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer.md) |
| `o` | the level, the dictionary, the check, the filters ([options](../xz-options.md)) |

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
    compress::xz::writer w(sink, {.dictionary = 1000});  // under 4 KiB
    auto wrote = w.write("hello");
    println("{}", wrote.error().message());
}
```

Output:

```text
write xz: invalid argument
```

## See also

- [xz::options](../xz-options.md)
- [sgcl::compress::xz::writer](../xz-writer.md)
