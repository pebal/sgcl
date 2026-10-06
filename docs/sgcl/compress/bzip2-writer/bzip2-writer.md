[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [writer](README.md)

# sgcl::compress::bzip2::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;             // (1)
writer(const io::writer& out, const options& o) noexcept;    // (2)
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet.

1. The default level: 9, blocks of 900 KB.
2. The options given.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer/README.md) |
| `o` | the level ([options](../bzip2-options.md)) |

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
    compress::bzip2::writer w(sink);
    w.write("hello");
    println("{} bytes before the close", sink.size());
    (void)w.close();
    println("{} bytes after it", sink.size());
}
```

Output:

```text
0 bytes before the close
39 bytes after it
```

## See also

- [bzip2::options](../bzip2-options.md)
- [sgcl::compress::bzip2::writer](README.md)
