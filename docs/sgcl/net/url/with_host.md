[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::with_host

```cpp
expected<url, io::error> with_host(const string& host) const noexcept;
```

The URL with the host given, by the standard's host setter, the pair of [host](host.md): a value with a port
(`example.com:8080`) sets both, so that what `host()` gives `with_host` takes, and a host is carried from one URL to
another by the two. A host followed by `:` and nothing (`"a:"`) keeps the port, as the standard's setter does. The
host goes through IDNA as the parser's does. Where the standard would set the host and refuse the port
(`"example.com:99999"`), the result is the error, and no URL.

## Parameters

| Parameter | Description |
|---|---|
| `host` | the new host, with a port or without one |

## Return value

The new URL, or an [io::error](../../io/error/README.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL host` and the value asked for, when the standard refuses the value or declines to apply it: a host
that does not parse (one that goes through IDNA is at most 1 MiB once decoded), a port that does not, or a URL with an
opaque path; and a value past 512 MiB, or a URL that would pass it ([the limit](README.md#rules)).

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
    println(u.with_host("B\xC3\xBC" "cher.de")->to_string());
    println(u.with_host("b.com:99")->to_string());
    println(u.with_host("b.com:")->to_string());
    for (const char* host : {"b c", "b.com:99999"}) {
        auto changed = u.with_host(host);
        println(changed ? changed->to_string() : changed.error().message());
    }
}
```

Output:

```text
http://xn--bcher-kva.de:81/x
http://b.com:99/x
http://b.com:81/x
set URL host b c: invalid URL
set URL host b.com:99999: invalid URL
```

## See also

- [host](host.md): the host and the port
- [with_hostname](with_hostname.md): the host alone
- [sgcl::net::url](README.md)
