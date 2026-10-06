[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::members

```cpp
slice<const member> members() const noexcept;
```

A map's [members](../cbor-member.md) in their order, a slice of the value; empty for every other kind.

## Parameters

None.

## Return value

The members.

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
    for (const auto& [key, value] : encoding::cbor::map({{1, "a"}, {"b", 2}}).members()) {
        println("{} = {}", key.to_string(), value.to_string());
    }
}
```

Output:

```text
1 = "a"
"b" = 2
```

## See also

- [elements](elements.md)
- [sgcl::encoding::cbor](README.md)
