[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip/README.md) › [reader](README.md)

# sgcl::compress::gzip::reader::reset

```cpp
void reset(const io::reader& in) noexcept;
```

Starts reading a new stream from `in`: the decoder's memory kept, nothing of the old stream carried over, the error
cleared, single_member kept. The old `in` is not closed.

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
    compress::gzip::reader r{io::buffer(compress::gzip::compress("first"))};
    println("{}", r.read_all_text().value_or(string()));
    r.reset(io::buffer(compress::gzip::compress("second")));
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
first
second
```

## See also

- [(constructor)](gzip-reader.md)
- [sgcl::compress::gzip::reader](README.md)
