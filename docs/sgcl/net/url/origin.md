[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::origin

```cpp
string origin() const noexcept;
```

The origin as the standard serializes it: the scheme, the host and the port, `https://example.com:8443`, for http,
https, ws, wss and ftp, and for a `blob:` URL of an http or https one; `null` for anything else, file among them. Go
has no counterpart.

## Parameters

None.

## Return value

The origin, or `null`.

## Complexity

Linear in the length of the URL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"https://example.com:8443/a?b", "http://example.com:80/", "blob:https://a.com/x",
                             "file:///x", "mailto:x"}) {
        println("{} -> {}", text, net::url(text).origin());
    }
}
```

Output:

```text
https://example.com:8443/a?b -> https://example.com:8443
http://example.com:80/ -> http://example.com
blob:https://a.com/x -> https://a.com
file:///x -> null
mailto:x -> null
```

## See also

- [host](host.md): the host and the port
- [sgcl::net::url](README.md)
