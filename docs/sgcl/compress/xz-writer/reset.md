[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz.md) › [writer](../xz-writer.md)

# sgcl::compress::xz::writer::reset

```cpp
void reset(const io::writer& out) noexcept;
```

Starts a new stream into `out` with the same options: the window and the tables kept, nothing of the old stream
carried over. The writer is open again and its error cleared. What the old stream did not close is dropped. A
program that writes many streams resets one writer and does not allocate the tables again.

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
    compress::xz::writer w{io::buffer()};
    for (string text : {"first", "second"}) {
        io::buffer sink;
        w.reset(sink);
        w.write(text);
        (void)w.close();
        compress::xz::reader r(sink);
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

- [(constructor)](xz-writer.md)
- [sgcl::compress::xz::writer](../xz-writer.md)
