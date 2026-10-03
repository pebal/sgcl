[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md) › [reader](../lzw-reader.md)

# sgcl::compress::lzw::reader::reset

```cpp
void reset(const io::reader& in) noexcept;
```

Starts reading a new stream from `in` with the same order and literal width: the decoder's memory kept, nothing of the
old stream carried over, the error cleared. The old `in` is not closed.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the new stream comes from |

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
    auto lsb = compress::lzw::order::lsb;
    compress::lzw::reader r{io::buffer(compress::lzw::compress("first", lsb, 8)), lsb, 8};
    println("{}", r.read_all_text().value_or(string()));
    r.reset(io::buffer(compress::lzw::compress("second", lsb, 8)));
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
first
second
```

## See also

- [(constructor)](lzw-reader.md)
- [sgcl::compress::lzw::reader](../lzw-reader.md)
