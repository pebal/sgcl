[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::effective_port

```cpp
uint16_t effective_port() const noexcept;
```

The port, or the scheme's default when none is written: 80 for http and ws, 443 for https and wss, 21 for ftp. The
port a connection to the URL goes to.

## Parameters

None.

## Return value

The port; 0 for a scheme without a default (file, and any scheme that is not special) and no port written.

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
    for (const char* text : {"http://x/", "wss://x/", "ftp://x/", "https://x:8443/", "file:///x", "sc://x/"}) {
        println("{} {}", text, net::url(text).effective_port());
    }
}
```

Output:

```text
http://x/ 80
wss://x/ 443
ftp://x/ 21
https://x:8443/ 8443
file:///x 0
sc://x/ 0
```

## See also

- [port](port.md): the port written
- [sgcl::net::url](../url.md)
