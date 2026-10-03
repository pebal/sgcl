[sgcl](../../README.md) › [net](../README.md) › [ip_network](../ip_network.md)

# sgcl::net::ip_network::operator==, operator\<=\>

```cpp
bool operator==(const ip_network&) const noexcept = default;     // (1)
auto operator<=>(const ip_network&) const noexcept = default;    // (2)
```

Compare two networks member by member: the address, as [ip_address](../ip_address/operator_cmp.md) orders addresses,
then the length. `!=`, `<`, `<=`, `>` and `>=` are made of these by the compiler. The address is compared as given:
`10.1.2.3/8` and `10.0.0.0/8` are two networks, equal after [masked](masked.md).

1. `true` when the addresses and the lengths are the same.
2. The order of the two.

## Parameters

| Parameter | Description |
|---|---|
| the operand | the network compared with `*this`, unnamed in the declarations |

## Return value

- (1) Whether the networks are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

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
    net::ip_network a("10.1.2.3/8"), b("10.0.0.0/8");
    println("{} {}", a == b, a.masked() == b);
    println("{}", net::ip_network("10.0.0.0/8") < net::ip_network("10.0.0.0/16"));
}
```

Output:

```text
false true
true
```

## See also

- [masked](masked.md): the first address of the network
- [sgcl::net::ip_network](../ip_network.md)
