[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::has_host

```cpp
bool has_host() const noexcept;
```

Checks whether the URL has a host: every special URL but a `file` one without it, and a URL of another scheme written
with `//`.

## Parameters

None.

## Return value

`true` when the URL has a host, an empty one among them.

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
    for (const char* text : {"https://example.com/", "file:///etc/hosts", "mailto:x@example.com", "sc://h/x"}) {
        println("{} {}", text, net::url(text).has_host());
    }
}
```

Output:

```text
https://example.com/ true
file:///etc/hosts true
mailto:x@example.com false
sc://h/x true
```

## See also

- [host](host.md): the host
- [sgcl::net::url](../url.md)
