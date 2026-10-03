[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::operator== (sgcl::io::error)

```cpp
friend bool operator==(const error& a, const error& b) noexcept;
```

Compares two errors by their codes: the operations and the paths play no part, so an error compares equal to one
made as a sentinel for its code. `!=` is the negation. A code compares by category as well as value, as
`error_code` does.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors to compare |

## Return value

`true` when the codes are equal.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const io::error closed(io::errc::closed, "");
    io::error got(io::errc::closed, "write", "socket");
    println("{} {}", got == closed, got == io::error(io::errc::unexpected_eof, "write", "socket"));
}
```

Output:

```text
true false
```

## See also

- [code](code.md)
- [sgcl::io::error](../error.md)
