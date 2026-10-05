[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::is_binary

```cpp
constexpr bool is_binary() const noexcept;
```

Whether the codec is the binary form ([binary](binary.md)).

## Parameters

None.

## Return value

`true` or `false`; `false` for `standard`.

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
    auto qp = encoding::quoted_printable::standard;
    println("{} {}", qp.is_binary(), qp.binary().lenient().is_binary());
}
```

Output:

```text
false true
```

## See also

- [binary](binary.md), [lenient](lenient.md)
- [quoted_printable](README.md)
