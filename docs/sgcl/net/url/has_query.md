[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::has_query

```cpp
bool has_query() const noexcept;
```

Checks whether the URL has a query: a `?`, followed by anything or nothing.

## Parameters

None.

## Return value

`true` when the URL has a `?`.

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
    for (const char* text : {"http://x/?a=1", "http://x/?", "http://x/"}) {
        println("{} {}", text, net::url(text).has_query());
    }
}
```

Output:

```text
http://x/?a=1 true
http://x/? true
http://x/ false
```

## See also

- [query](query.md): the query
- [sgcl::net::url](README.md)
