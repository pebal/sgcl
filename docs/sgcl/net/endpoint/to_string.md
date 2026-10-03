[sgcl](../../README.md) › [net](../README.md) › [endpoint](../endpoint.md)

# sgcl::net::endpoint::to_string

```cpp
string to_string() const noexcept;
```

The text of the endpoint, Go's `AddrPort.String`: the address as [ip_address::to_string](../ip_address/to_string.md)
writes it, in brackets when it is IPv6, `:` and the port in decimal; `invalid AddrPort` for the empty endpoint, Go's
text.

## Parameters

None.

## Return value

The text, at most `MaxText` (63) bytes.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::endpoint(net::ip_address("10.0.0.1"), 80).to_string());
    println(net::endpoint(net::ip_address("2001:DB8::1"), 443).to_string());
    println(net::endpoint().to_string());
}
```

Output:

```text
10.0.0.1:80
[2001:db8::1]:443
invalid AddrPort
```

## See also

- [write_text](write_text.md): the same text with no string made
- [parse](parse.md): the text read
- [sgcl::net::endpoint](../endpoint.md)
