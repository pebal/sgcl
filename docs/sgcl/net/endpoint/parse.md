[sgcl](../../README.md) › [net](../README.md) › [endpoint](README.md)

# sgcl::net::endpoint::parse

```cpp
static expected<endpoint, io::error> parse(const string& text) noexcept;
```

Reads an endpoint, `address:port`, as Go's `netip.ParseAddrPort` reads one: `"1.2.3.4:80"`, `"[::1]:443"`,
`"[fe80::1%en0]:80"`. The text is taken apart at its last `:`; the address is read as
[ip_address::parse](../ip_address/parse.md) reads one, an IPv6 address in brackets and an IPv4 address without them,
so `"::1:80"` and `"[1.2.3.4]:80"` are both refused. Any byte of a zone is the zone's, a bracket included:
`[fe80::1%a]b]:80` is an endpoint. The port is decimal, 0 to 65535, leading zeros accepted, a sign not.

A name is not an endpoint: `"localhost:80"` is refused, and goes to [tcp::connect](../tcp/connect.md), which resolves
it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |

## Return value

The endpoint, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_address` ([errc](../errc.md)), the
operation `parse endpoint` and the text.

## Complexity

Linear in the length of `text`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"1.2.3.4:80", "[::1]:443", "[fe80::1%en0]:080", "[fe80::1%a]b]:80", "::1:80",
                             "[1.2.3.4]:80", "1.2.3.4:65536", "1.2.3.4:+80", "localhost:80"}) {
        auto e = net::endpoint::parse(text);
        println("{} -> {}", text, e ? e->to_string() : e.error().message());
    }
}
```

Output:

```text
1.2.3.4:80 -> 1.2.3.4:80
[::1]:443 -> [::1]:443
[fe80::1%en0]:080 -> [fe80::1%en0]:80
[fe80::1%a]b]:80 -> [fe80::1%a]b]:80
::1:80 -> parse endpoint ::1:80: invalid address
[1.2.3.4]:80 -> parse endpoint [1.2.3.4]:80: invalid address
1.2.3.4:65536 -> parse endpoint 1.2.3.4:65536: invalid address
1.2.3.4:+80 -> parse endpoint 1.2.3.4:+80: invalid address
localhost:80 -> parse endpoint localhost:80: invalid address
```

## See also

- [(constructor)](endpoint.md): the endpoint a literal spells
- [to_string](to_string.md): the text back
- [sgcl::net::endpoint](README.md)
