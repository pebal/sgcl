[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::has_zone

```cpp
bool has_zone() const noexcept;
```

Checks whether the address has a zone. Only an IPv6 address may have one.

## Parameters

None.

## Return value

`true` when the zone is not empty.

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
    println("{} {}", net::ip_address("fe80::1%en0").has_zone(), net::ip_address("fe80::1").has_zone());
}
```

Output:

```text
true false
```

## See also

- [zone](zone.md): the zone itself
- [sgcl::net::ip_address](../ip_address.md)
