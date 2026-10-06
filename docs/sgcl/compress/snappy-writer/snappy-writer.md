[sgcl](../../README.md) › [compress](../README.md) › [snappy](../snappy/README.md) › [writer](README.md)

# sgcl::compress::snappy::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;
```

Constructs a writer that compresses what is written to it into `out`. Nothing is written yet: the first write writes
the stream identifier.

Snappy has no levels and no options.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the compressed bytes go to: any [io writer](../../io/writer/README.md) |

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
    compress::snappy::writer w(sink);
    w.write("hello");
    println("{} bytes before the close", sink.size());
    (void)w.close();
    println("{} bytes after it", sink.size());
}
```

Output:

```text
10 bytes before the close
23 bytes after it
```

## See also

- [sgcl::compress::snappy::writer](README.md)
