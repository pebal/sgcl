[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::asn1::oid::starts_with

```cpp
bool starts_with(const oid& prefix) const noexcept;
```

Whether the arcs of `prefix` are the first arcs of this identifier: whether it lies in the subtree `prefix`
names (`1.3.6.1.4.1.311` and everything Microsoft registers under it). Every identifier starts with itself and
with `oid()`.

## Parameters

| Parameter | Description |
|---|---|
| `prefix` | the arcs looked for |

## Return value

`true` when `prefix` is a prefix.

## Complexity

Linear in the size of `prefix`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1::oid id("1.3.6.1.4.1.311.21.20");
    println(id.starts_with(encoding::asn1::oid("1.3.6.1.4.1.311")));
    println(id.starts_with(encoding::asn1::oid("1.3.6.1.4.1.31")));
}
```

Output:

```text
true
false
```

## See also

- [operator==, operator\<=\>](operator_cmp.md)
- [sgcl::encoding::asn1::oid](README.md)
