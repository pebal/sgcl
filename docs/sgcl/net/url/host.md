[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::host

```cpp
string host() const noexcept;
```

The host and the port when one is written, as the standard's `host` getter and Go's `URL.Host` have it:
`example.com:8443`, `[::1]:8080`, `xn--bcher-kva.de`, `10.0.0.1`. An IPv6 host is in its brackets, written as the
standard writes it: the first longest run of zero pieces as `::`, with no dotted tail (`[::ffff:102:304]`, where RFC
5952 writes `::ffff:1.2.3.4`). What `host()` gives, [with_host](with_host.md) takes.

## Parameters

None.

## Return value

The host and the port; the empty string when there is no host, and for the empty host of `file:///x`.

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
    for (const char* text : {"https://example.com:8443/", "http://[::1]:8080/", "http://example.com:80/",
                             "http://[::ffff:1.2.3.4]/", "file:///etc/hosts"}) {
        println("[{}]", net::url(text).host());
    }
}
```

Output:

```text
[example.com:8443]
[[::1]:8080]
[example.com]
[[::ffff:102:304]]
[]
```

## See also

- [hostname](hostname.md): the host alone
- [with_host](with_host.md): another host
- [sgcl::net::url](../url.md)
