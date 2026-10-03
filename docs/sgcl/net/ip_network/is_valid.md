[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::ip_network::is_valid

```cpp
bool is_valid() const noexcept;
```

Checks whether the value is a network, not the empty one; Go's `Prefix.IsValid`.

## Parameters

None.

## Return value

`true` for a network of a valid address and a length, `false` for the empty network.

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
    net::ip_network none;
    println("{} {}", none.is_valid(), net::ip_network("10.0.0.0/8").is_valid());
    println("{}", net::ip_network(net::ip_address(), 8).is_valid());  // the empty address
}
```

Output:

```text
false true
false
```

## See also

- [bits](bits.md): -1 for the empty network
- [sgcl::net::ip_network](README.md)
