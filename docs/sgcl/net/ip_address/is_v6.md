[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::is_v6

```cpp
bool is_v6() const noexcept;
```

Checks whether the address is IPv6, an IPv4-mapped one among them; Go's `Addr.Is6`.

## Parameters

None.

## Return value

`true` for an IPv6 address, `false` for an IPv4 one and for the empty one.

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
    println("{} {} {}", net::ip_address("2001:db8::1").is_v6(), net::ip_address("::ffff:1.2.3.4").is_v6(),
            net::ip_address("1.2.3.4").is_v6());
}
```

Output:

```text
true true false
```

## See also

- [is_v4](is_v4.md), [is_v4_mapped](is_v4_mapped.md): the other kinds
- [sgcl::net::ip_address](README.md)
