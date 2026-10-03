[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::any_v6

```cpp
static ip_address any_v6() noexcept;
```

The address `::`, the unspecified address of IPv6: a socket bound to it listens on every interface of the family; Go's
`netip.IPv6Unspecified()`.

## Parameters

None.

## Return value

The address `::`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto a = net::ip_address::any_v6();
    println("{} {} {}", a, a.is_v6(), a.is_unspecified());
}
```

Output:

```text
:: true true
```

## See also

- [any_v4](any_v4.md): the same of the other family
- [is_unspecified](is_unspecified.md): the predicate
- [sgcl::net::ip_address](../ip_address.md)
