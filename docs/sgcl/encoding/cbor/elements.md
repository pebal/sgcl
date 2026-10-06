[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::elements

```cpp
slice<const cbor> elements() const noexcept;
```

An array's elements, a slice of the value, nothing copied; empty for every other kind.

## Parameters

None.

## Return value

The elements.

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
    for (const auto& e : encoding::cbor::array({1, "a", true}).elements()) {
        println(e.to_string());
    }
}
```

Output:

```text
1
"a"
true
```

## See also

- [members](members.md)
- [sgcl::encoding::cbor](README.md)
