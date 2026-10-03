[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::ip_address::is_valid

```cpp
bool is_valid() const noexcept;
```

Checks whether the value is an address, not the empty one the default constructor makes; Go's `Addr.IsValid`.

## Parameters

None.

## Return value

`true` for an IPv4 or an IPv6 address, `false` for the empty one.

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
    net::ip_address none;
    println("{} {}", none.is_valid(), net::ip_address("::1").is_valid());
    println("{}", net::ip_address::v4(255, 255, 255, 255).next().is_valid());  // past the last
}
```

Output:

```text
false true
false
```

## See also

- [(constructor)](ip_address.md): the empty address
- [sgcl::net::ip_address](../ip_address.md)
