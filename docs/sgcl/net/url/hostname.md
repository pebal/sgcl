[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::hostname

```cpp
string hostname() const noexcept;
```

The host alone, without the port and without the brackets of an IPv6 address, as Go's `URL.Hostname()`: `example.com`,
`::1`. The standard's `hostname` getter keeps the brackets; [with_hostname](with_hostname.md) takes an IPv6 address in
them.

## Parameters

None.

## Return value

The host, or the empty string when there is none.

## Complexity

Linear in the length of the part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    println(net::url("https://example.com:8443/").hostname());
    println(net::url("http://[::1]:8080/").hostname());
}
```

Output:

```text
example.com
::1
```

## See also

- [host](host.md): the host with the port
- [host_address](host_address.md): the host as an address
- [sgcl::net::url](../url.md)
