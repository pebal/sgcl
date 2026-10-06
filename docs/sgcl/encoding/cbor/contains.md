[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::contains

```cpp
bool contains(const cbor& key) const noexcept;
```

Whether a map has the key, by deep equality; `false` for every other kind.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the map has it.

## Complexity

Linear in the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor m = encoding::cbor::map({{1, encoding::cbor()}});
    println("{} {} {}", m.contains(1), m.contains(2), m.contains(1.0));
}
```

Output:

```text
true false false
```

## See also

- [operator\\[\\]](operator_at.md)
- [sgcl::encoding::cbor](README.md)
