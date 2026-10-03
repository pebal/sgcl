[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::has_opaque_path

```cpp
bool has_opaque_path() const noexcept;
```

Checks whether the URL has an opaque path, not a hierarchical one: `mailto:x`, `data:,x`. Only a scheme that is not
special may have one; a URL with an opaque path takes no host and no new path ([with_host](with_host.md),
[with_path](with_path.md)).

## Parameters

None.

## Return value

`true` for an opaque path.

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
    for (const char* text : {"mailto:x@example.com", "data:,hello", "sc://h/x", "https://x/"}) {
        println("{} {}", text, net::url(text).has_opaque_path());
    }
}
```

Output:

```text
mailto:x@example.com true
data:,hello true
sc://h/x false
https://x/ false
```

## See also

- [path](path.md): the path
- [sgcl::net::url](../url.md)
