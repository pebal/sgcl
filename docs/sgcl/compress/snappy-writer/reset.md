[sgcl](../../README.md) › [compress](../README.md) › [snappy](../snappy/README.md) › [writer](README.md)

# sgcl::compress::snappy::writer::reset

```cpp
void reset(const io::writer& out) noexcept;
```

Starts a new stream into `out`: the buffers and the table kept, nothing of the old stream
carried over. The writer is open again and its error cleared. What the old stream did not close is dropped. A
program that writes many streams resets one writer and does not allocate again.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the new stream goes to |

## Return value

None.

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
    compress::snappy::writer w{io::buffer()};
    for (string text : {"first", "second"}) {
        io::buffer sink;
        w.reset(sink);
        w.write(text);
        (void)w.close();
        compress::snappy::reader r(sink);
        println("{}", r.read_all_text().value_or(string()));
    }
}
```

Output:

```text
first
second
```

## See also

- [(constructor)](snappy-writer.md)
- [sgcl::compress::snappy::writer](README.md)
