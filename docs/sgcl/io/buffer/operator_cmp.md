[sgcl](../../README.md) › [io](../README.md) › [buffer](../buffer.md)

# sgcl::io::operator== (sgcl::io::buffer)

```cpp
friend bool operator==(const buffer& a, const buffer& b) noexcept;
```

Checks whether `a` and `b` are the same buffer: whether the two handles hold the same state. Two buffers with the
same bytes are not equal; a copy of a handle is. `!=` is the negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the buffers to compare |

## Return value

`true` when both handles hold the same buffer.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer a("same bytes");
    io::buffer b("same bytes");
    io::buffer c = a;
    println("{} {} {}", a == b, a == c, a.text() == b.text());
}
```

Output:

```text
false true true
```

## See also

- [operator=](operator_assign.md)
- [sgcl::io::buffer](../buffer.md)
