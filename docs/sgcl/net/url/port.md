[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::port

```cpp
optional<uint16_t> port() const noexcept;
```

The port written in the URL. The parser drops the scheme's default (`:80` for http), so `http://x:80/` has none. Go's
`URL.Port()`, which gives the port as a text and keeps a default one written.

## Parameters

None.

## Return value

The port, or `nullopt` when none was written or it was the scheme's default.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"http://x:8080/", "http://x:80/", "https://x/"}) {
        auto p = net::url(text).port();
        println("{} {}", text, p ? to_string(*p) : string("none"));
    }
}
```

Output:

```text
http://x:8080/ 8080
http://x:80/ none
https://x/ none
```

## See also

- [effective_port](effective_port.md): the port or the default
- [with_port](with_port.md): another port
- [sgcl::net::url](README.md)
