[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::any_v4

```cpp
static ip_address any_v4() noexcept;
```

The address `0.0.0.0`, the unspecified address of IPv4: a socket bound to it listens on every interface of the family;
Go's `netip.IPv4Unspecified()`.

## Parameters

None.

## Return value

The address `0.0.0.0`.

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
    auto a = net::ip_address::any_v4();
    println("{} {} {}", a, a.is_v4(), a.is_unspecified());
}
```

Output:

```text
0.0.0.0 true true
```

## See also

- [any_v6](any_v6.md): the same of the other family
- [is_unspecified](is_unspecified.md): the predicate
- [sgcl::net::ip_address](../ip_address.md)
