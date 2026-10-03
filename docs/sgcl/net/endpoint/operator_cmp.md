[sgcl](../../README.md) › [net](../README.md) › [endpoint](README.md)

# sgcl::net::endpoint::operator==, operator\<=\>

```cpp
bool operator==(const endpoint&) const noexcept = default;     // (1)
auto operator<=>(const endpoint&) const noexcept = default;    // (2)
```

Compare two endpoints member by member: the address, as [ip_address](../ip_address/operator_cmp.md) orders addresses,
then the port; Go's `AddrPort.Compare`. `!=`, `<`, `<=`, `>` and `>=` are made of these by the compiler.

1. `true` when the addresses and the ports are the same.
2. The order of the two.

## Parameters

| Parameter | Description |
|---|---|
| the operand | the endpoint compared with `*this`, unnamed in the declarations |

## Return value

- (1) Whether the endpoints are equal.
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
    println("{}", net::endpoint("10.0.0.1:80") == net::endpoint("10.0.0.1:0080"));
    println("{}", net::endpoint("10.0.0.1:443") < net::endpoint("10.0.0.2:80"));
    println("{}", net::endpoint("10.0.0.1:80") < net::endpoint("10.0.0.1:443"));
}
```

Output:

```text
true
true
true
```

## See also

- [ip_address::operator==, operator\<=\>](../ip_address/operator_cmp.md): the order of addresses
- [sgcl::net::endpoint](README.md)
