[sgcl](../../README.md) › [compress](../README.md) › [flate](../flate/README.md) › [reader](README.md)

# sgcl::compress::flate::reader::reset

```cpp
void reset(const io::reader& in) noexcept;
```

Starts reading a new stream from `in`: the decoder's memory kept, nothing of the old stream carried over, the error
cleared, the options of the constructor kept. The old `in` is not closed.

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
    compress::flate::reader r{io::buffer(compress::flate::compress("first"))};
    println("{}", r.read_all_text().value_or(string()));
    r.reset(io::buffer(compress::flate::compress("second")));
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
first
second
```

## See also

- [(constructor)](flate-reader.md)
- [sgcl::compress::flate::reader](README.md)
