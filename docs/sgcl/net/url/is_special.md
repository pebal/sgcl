[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::is_special

```cpp
bool is_special() const noexcept;
```

Checks whether the scheme is one the standard gives a meaning of its own: `http`, `https`, `ws`, `wss`, `ftp`, `file`.
A special URL has a host (but for file), a hierarchical path, `\` read as `/` and its default port dropped.

## Parameters

None.

## Return value

`true` for a special scheme.

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
    for (const char* text : {"https://x/", "file:///x", "mailto:x", "sc://x/"}) {
        println("{} {}", text, net::url(text).is_special());
    }
}
```

Output:

```text
https://x/ true
file:///x true
mailto:x false
sc://x/ false
```

## See also

- [scheme](scheme.md): the scheme
- [sgcl::net::url](../url.md)
