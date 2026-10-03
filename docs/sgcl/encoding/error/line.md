[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::line

```cpp
uint32_t line() const noexcept;
```

The line of the input the error was found on, from 1, counted after the fact, when something failed: a format of
lines (PEM, CSV, JSON, XML) gives it, a format without lines (base64 of one string, varint) leaves it 0, and so does
an error made by a program until [locate](locate.md) or [set_position](set_position.md) gives it.
[message()](message.md) shows `line:column` when the line is known, and the offset when it is 0.

## Parameters

None.

## Return value

The line, from 1; 0 when it is not known.

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
    auto block = encoding::pem::parse("A comment.\n"
                                      "-----BEGIN A-----\nQUJD\nQU*D\n-----END A-----\n");
    println("{} {}", block.error().line(), block.error().message());

    auto bytes = encoding::base64::standard.decode("QU*D");
    println("{} {}", bytes.error().line(), bytes.error().message());
}
```

Output:

```text
4 4:3: invalid character '*'
0 offset 2: invalid character '*'
```

## See also

- [column](column.md): the column on the line
- [locate](locate.md): the line and the column of the offset in a text
- [sgcl::encoding::error](../error.md)
