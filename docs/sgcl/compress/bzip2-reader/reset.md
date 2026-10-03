[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [reader](README.md)

# sgcl::compress::bzip2::reader::reset

```cpp
void reset(const io::reader& in) noexcept;
```

Starts reading a new stream from `in`: the decoder's memory (some 3.6 MB for the blocks of 900 KB) kept, nothing of
the old stream carried over, the error cleared. The old `in` is not closed.

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto first = encoding::hex::decode("425a6839314159265359b2390bdd000000018001201c0020002183"
                                       "419a025c7177245385090b2390bdd0");
    auto second = encoding::hex::decode("425a6839314159265359268a023400000281800e01880020002218"
                                        "68300702985dc914e142409a2808d0");
    compress::bzip2::reader r{io::buffer(*first)};
    println("{}", r.read_all_text().value_or(string()));
    r.reset(io::buffer(*second));
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
first
second
```

## See also

- [(constructor)](bzip2-reader.md)
- [sgcl::compress::bzip2::reader](README.md)
