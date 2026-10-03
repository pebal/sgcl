[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz/README.md) › [reader](README.md)

# sgcl::compress::xz::reader::reset

```cpp
void reset(const io::reader& in) noexcept;
```

Starts reading a new stream from `in`: the dictionary kept when it is large enough for the new stream, nothing of the
old stream carried over, the error cleared, the limits of the constructor kept. The old `in` is not closed.

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
    compress::xz::reader r{io::buffer(compress::xz::compress("first"))};
    println("{}", r.read_all_text().value_or(string()));
    r.reset(io::buffer(compress::xz::compress("second")));
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
first
second
```

## See also

- [(constructor)](xz-reader.md)
- [sgcl::compress::xz::reader](README.md)
