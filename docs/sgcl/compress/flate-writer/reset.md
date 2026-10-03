[sgcl](../../README.md) › [compress](../README.md) › [flate](../flate.md) › [writer](../flate-writer.md)

# sgcl::compress::flate::writer::reset

```cpp
void reset(const io::writer& out) noexcept;
```

Starts a new stream into `out` with the same options: the encoder's memory kept, nothing of the old stream carried over
(the dictionary given to the constructor is the history again). The writer is open again and its error cleared. What the
old stream did not close is dropped, as the destructor drops it.

A program that writes many small streams (a response each) resets one writer instead of making a new one, and does
not allocate the encoder's tables again.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the new stream goes to |

## Return value

None.

## Complexity

Constant; the encoder's tables are cleared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::flate::writer w{io::buffer()};
    for (string text : {"first", "second"}) {
        io::buffer sink;
        w.reset(sink);
        w.write(text);
        (void)w.close();
        compress::flate::reader r(sink);
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

- [(constructor)](flate-writer.md)
- [sgcl::compress::flate::writer](../flate-writer.md)
