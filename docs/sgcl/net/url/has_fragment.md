[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::has_fragment

```cpp
bool has_fragment() const noexcept;
```

Checks whether the URL has a fragment: a `#`, followed by anything or nothing.

## Parameters

None.

## Return value

`true` when the URL has a `#`.

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
    for (const char* text : {"http://x/#top", "http://x/#", "http://x/"}) {
        println("{} {}", text, net::url(text).has_fragment());
    }
}
```

Output:

```text
http://x/#top true
http://x/# true
http://x/ false
```

## See also

- [fragment](fragment.md): the fragment
- [sgcl::net::url](../url.md)
