[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::is_lenient

```cpp
constexpr bool is_lenient() const noexcept;
```

Whether the codec's decoding is lenient ([lenient](lenient.md)).

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
    println("{} {}", qp.is_lenient(), qp.binary().lenient().is_lenient());
}
```

Output:

```text
false true
```

## See also

- [binary](binary.md), [lenient](lenient.md)
- [quoted_printable](README.md)
