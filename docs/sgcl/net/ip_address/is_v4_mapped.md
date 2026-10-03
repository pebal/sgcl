[sgcl](../../README.md) › [net](../README.md) › [ip_address](README.md)

# sgcl::net::ip_address::is_v4_mapped

```cpp
bool is_v4_mapped() const noexcept;
```

Checks whether the address is an IPv4-mapped IPv6 address, `::ffff:a.b.c.d`: the form in which a dual-stack socket
sees an IPv4 peer. Go's `Addr.Is4In6`.

## Parameters

None.

## Return value

`true` for an IPv6 address of the prefix `::ffff:0:0/96`, `false` otherwise.

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
    println("{} {}", net::ip_address("::ffff:10.0.0.1").is_v4_mapped(),
            net::ip_address("10.0.0.1").is_v4_mapped());
}
```

Output:

```text
true false
```

## See also

- [unmap](unmap.md): the IPv4 address of a mapped one
- [sgcl::net::ip_address](README.md)
