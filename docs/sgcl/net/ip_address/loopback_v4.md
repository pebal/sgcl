[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::loopback_v4

```cpp
static ip_address loopback_v4() noexcept;
```

The address `127.0.0.1`, the loopback address of IPv4: the host itself; Go writes it `netip.AddrFrom4([4]byte{127, 0,
0, 1})`.

## Parameters

None.

## Return value

The address `127.0.0.1`.

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
    auto a = net::ip_address::loopback_v4();
    println("{} {} {}", a, a.is_v4(), a.is_loopback());
}
```

Output:

```text
127.0.0.1 true true
```

## See also

- [loopback_v6](loopback_v6.md): the same of the other family
- [is_loopback](is_loopback.md): the predicate
- [sgcl::net::ip_address](../ip_address.md)
