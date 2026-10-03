[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::column

```cpp
uint32_t column() const noexcept;
```

The column of the error on its [line](line.md), from 1, in code points, not bytes: it is read by a person looking at
the line, where `ż` is one character. The byte is [offset()](offset.md). Go counts the columns of its CSV errors in
bytes; the two are the same for ASCII.

## Parameters

None.

## Return value

The column, from 1; 0 when the line is not known.

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
    encoding::csv::reader r("miasto,ulica\nŁódź,\"Piotrkowska\"x\n");
    while (r.next()) {
    }
    const auto& e = r.last_error().value();
    println("line {}, column {}, byte {}", e.line(), e.column(), e.offset());
}
```

Output:

```text
line 2, column 18, byte 33
```

## See also

- [line](line.md): the line
- [offset](offset.md): the byte
- [sgcl::encoding::error](../error.md)
