[sgcl](../../README.md) › [compress](../README.md) › [snappy](../snappy/README.md) › [reader](README.md)

# sgcl::compress::snappy::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;
```

Constructs a reader of what the data read from `in` decompresses to. Nothing is read yet: the first read reads the
header of the stream.

A reader of the framed data `in` gives.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the compressed bytes come from |

## Complexity

Constant; the input buffer is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::snappy::compress("hello");
    compress::snappy::reader r{io::buffer(packed)};
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
hello
```

## See also

- [read](read.md)
- [sgcl::compress::snappy::reader](README.md)
