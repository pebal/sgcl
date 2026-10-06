[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::operator==, operator\\<=\\> (sgcl::encoding::uuid)

```cpp
friend constexpr bool operator==(const uuid& a, const uuid& b) noexcept;                     // (1)
friend constexpr std::strong_ordering operator<=>(const uuid& a, const uuid& b) noexcept;    // (2)
```

1. Whether the two are the same sixteen bytes.
2. The order of the bytes, as RFC 9562 §6.11 compares UUIDs: v7s by the time they were made.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the UUIDs |

## Return value

(1) `true` for the same bytes; (2) their order.

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
    encoding::uuid a("00000000-0000-7000-8000-000000000001");
    encoding::uuid b("00000000-0000-7000-8000-000000000002");
    println("{} {} {}", a < b, a == b, a == encoding::uuid("00000000000070008000000000000001"));
}
```

Output:

```text
true false true
```

## See also

- [hash](hash.md)
- [sgcl::encoding::uuid](README.md)
