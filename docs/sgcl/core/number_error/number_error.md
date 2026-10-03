[sgcl](../../README.md) › [core](../README.md) › [number_error](../number_error.md)

# sgcl::number_error::number_error

```cpp
constexpr number_error(reason r, size_t offset) noexcept;
```

Constructs an error of the reason `r` at the byte `offset` of a text. [parse](../parse.md) makes one for a text it
reads no number from; a program makes one to compare with, or to report a number of its own reading the same way.

## Parameters

| Parameter | Description |
|---|---|
| `r` | why the text is not a number ([reason](../number_error-reason.md)) |
| `offset` | the byte of the text where the reading stopped |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    number_error e(number_error::reason::trailing, 2);
    println("{} at {}", e.message(), e.offset());
    println("{}", parse<int>("12px").error() == e);
}
```

Output:

```text
more after the number at 2
true
```

## See also

- [why](why.md), [offset](offset.md): the two fields
- [sgcl::number_error](../number_error.md)
