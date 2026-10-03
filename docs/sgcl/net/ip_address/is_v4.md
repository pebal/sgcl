[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::is_v4

```cpp
bool is_v4() const noexcept;
```

Checks whether the address is IPv4; Go's `Addr.Is4`. An IPv4-mapped IPv6 address is not: it is IPv6, and
[unmap](unmap.md) gives its IPv4 address.

## Parameters

None.

## Return value

`true` for an IPv4 address, `false` for an IPv6 one and for the empty one.

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
    println("{} {}", net::ip_address("1.2.3.4").is_v4(), net::ip_address("::ffff:1.2.3.4").is_v4());
}
```

Output:

```text
true false
```

## See also

- [is_v6](is_v6.md), [is_v4_mapped](is_v4_mapped.md): the other kinds
- [sgcl::net::ip_address](README.md)
