[sgcl](../../README.md) › [encoding](../README.md) › [error](README.md)

# sgcl::encoding::error::set_position

```cpp
error& set_position(uint32_t line, uint32_t column) noexcept;
```

Sets the line and the column as they are given, for a reader that counts them itself as it goes — a reader of a
stream, which no longer has the text before the error to [locate](locate.md) the offset in. The offset stays as it
was.

## Parameters

| Parameter | Description |
|---|---|
| `line` | the line, from 1; 0 for none |
| `column` | the column, from 1, in code points |

## Return value

`*this`.

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
    encoding::error e(encoding::errc::unexpected_end, 4096, "the file ends inside a record");
    println("{}", e.set_position(120, 7).message());
    println("{}", e.set_position(0, 0).message());
}
```

Output:

```text
120:7: the file ends inside a record
offset 4096: the file ends inside a record
```

## See also

- [locate](locate.md): the line and the column counted in a text
- [sgcl::encoding::error](README.md)
