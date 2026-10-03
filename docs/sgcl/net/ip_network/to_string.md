[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::ip_network::to_string

```cpp
string to_string() const noexcept;
```

The text of the network, Go's `Prefix.String`: the address as [ip_address::to_string](../ip_address/to_string.md)
writes it, `/` and the length in decimal; `invalid Prefix` for the empty network, Go's text.

## Parameters

None.

## Return value

The text, at most `MaxText` (59) bytes.

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
    println(net::ip_network("2001:0DB8:0000::/32").to_string());
    println(net::ip_network("10.1.2.3/8").to_string());
    println(net::ip_network().to_string());
}
```

Output:

```text
2001:db8::/32
10.1.2.3/8
invalid Prefix
```

## See also

- [write_text](write_text.md): the same text with no string made
- [parse](parse.md): the text read
- [sgcl::net::ip_network](README.md)
