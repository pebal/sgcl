[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw/README.md) › [writer](README.md)

# sgcl::compress::lzw::writer::reset

```cpp
void reset(const io::writer& out) noexcept;
```

Starts a new stream into `out` with the same order and literal width: the encoder's table kept and cleared, nothing of
the old stream carried over. The writer is open again and its error cleared. What the old stream did not close is
dropped. A program that writes many streams resets one writer and does not allocate the table again.

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
    compress::lzw::writer w(io::buffer(), compress::lzw::order::msb, 8);
    for (string text : {"first", "second"}) {
        io::buffer sink;
        w.reset(sink);
        w.write(text);
        (void)w.close();
        compress::lzw::reader r(sink, compress::lzw::order::msb, 8);
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

- [(constructor)](lzw-writer.md)
- [sgcl::compress::lzw::writer](README.md)
