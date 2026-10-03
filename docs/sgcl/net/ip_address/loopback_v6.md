[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::loopback_v6

```cpp
static ip_address loopback_v6() noexcept;
```

The address `::1`, the loopback address of IPv6: the host itself; Go's `netip.IPv6Loopback()`.

## Parameters

None.

## Return value

The address `::1`.

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
    auto a = net::ip_address::loopback_v6();
    println("{} {} {}", a, a.is_v6(), a.is_loopback());
}
```

Output:

```text
::1 true true
```

## See also

- [loopback_v4](loopback_v4.md): the same of the other family
- [is_loopback](is_loopback.md): the predicate
- [sgcl::net::ip_address](../ip_address.md)
