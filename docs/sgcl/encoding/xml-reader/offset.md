[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [reader](README.md)

# sgcl::encoding::xml::reader::offset

```cpp
uint64_t offset() const noexcept;
```

The byte of the input where the next token starts, counted in the input as it was (a document in UTF-16 counts
its own bytes): Go's `Decoder.InputOffset`. The line and the column are counted only for an error, which has them.

## Parameters

None.

## Return value

The offset of the next token.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<a><b>text</b></a>");
    while (r.peek()) {
        uint64_t at = r.offset();
        println("{} {}", at, int(r.next()->type()));
    }
}
```

Output:

```text
0 0
3 0
6 2
10 1
14 1
```

## See also

- [depth](depth.md): the elements open around the next token
- [sgcl::encoding::xml::reader](README.md)
