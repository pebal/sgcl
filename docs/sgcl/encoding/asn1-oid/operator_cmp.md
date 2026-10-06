[sgcl](../../README.md) › [encoding](../README.md) › [asn1](../asn1/README.md) › [oid](README.md)

# sgcl::encoding::operator==, operator\<=\> (sgcl::encoding::asn1::oid)

```cpp
friend bool operator==(const oid& a, const oid& b) noexcept;                     // (1)
friend std::strong_ordering operator<=>(const oid& a, const oid& b) noexcept;    // (2)
```

1. Whether the two are the same arcs.
2. The order arc by arc, a prefix before what it is a prefix of: `1.2 < 1.2.3 < 1.10 < 2.0`. It is the order of
   the DER bytes, compared as they are.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the identifiers |

## Return value

(1) `true` for the same arcs; (2) their order.

## Complexity

Linear in the size of the identifiers.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::asn1::oid a("1.2.3"), b("1.10");
    println("{} {} {}", a < b, a == encoding::asn1::oid({1, 2, 3}), encoding::asn1::oid("1.2") < a);
}
```

Output:

```text
true true true
```

## See also

- [starts_with](starts_with.md)
- [sgcl::encoding::asn1::oid](README.md)
