[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [writer](README.md)

# sgcl::compress::bzip2::writer::reset

```cpp
void reset(const io::writer& out) noexcept;
```

Starts a new stream into `out` with the same options: the buffers and the tables kept, nothing of the old stream
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
    compress::bzip2::writer w{io::buffer()};
    for (string text : {"first", "second"}) {
        io::buffer sink;
        w.reset(sink);
        w.write(text);
        (void)w.close();
        compress::bzip2::reader r(sink);
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

- [(constructor)](bzip2-writer.md)
- [sgcl::compress::bzip2::writer](README.md)
