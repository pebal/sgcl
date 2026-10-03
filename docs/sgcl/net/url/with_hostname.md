[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::with_hostname

```cpp
expected<url, io::error> with_hostname(const string& hostname) const noexcept;
```

The URL with the host given and the port kept, by the standard's hostname setter. An IPv6 address is given in its
brackets, `[::1]`, where [hostname](hostname.md) gives it without them, as Go's `Hostname()` does; a host is carried
from one URL to another by [host](host.md) and [with_host](with_host.md).

## Parameters

| Parameter | Description |
|---|---|
| `hostname` | the new host, without a port; an IPv6 address in brackets |

## Return value

The new URL, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL hostname` and the value asked for, when the standard refuses the value or declines to apply it: a
host that does not parse (one that goes through IDNA is at most 1 MiB once decoded), a host given with a port, or a
URL with an opaque path; and a value past 512 MiB, or a URL that would pass it ([the limit](../url.md#rules)).

## Complexity

Linear in the length of the URL and of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("http://a.com:81/x");
    println(u.with_hostname("b.com")->to_string());
    println(u.with_hostname("[::1]")->to_string());
    auto port = u.with_hostname("b.com:99");
    println(port.error().message());
}
```

Output:

```text
http://b.com:81/x
http://[::1]:81/x
set URL hostname b.com:99: invalid URL
```

## See also

- [hostname](hostname.md): the host alone
- [with_host](with_host.md): the host and a port
- [sgcl::net::url](../url.md)
