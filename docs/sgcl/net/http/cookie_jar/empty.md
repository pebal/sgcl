[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::empty

```cpp
bool empty() const noexcept;
```

Whether the jar holds no cookie: [size](size.md) is 0.

## Parameters

None.

## Return value

`true` when it holds none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::cookie_jar jar;
    println("{}", jar.empty());
    jar.set_cookies(net::url("https://example.com/"), {net::http::cookie("a=1")});
    println("{}", jar.empty());
}
```

Output:

```text
true
false
```

## See also

- [size](size.md): how many
- [sgcl::net::http::cookie_jar](README.md)
