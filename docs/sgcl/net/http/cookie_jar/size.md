[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::size

```cpp
size_t size() const noexcept;
```

The cookies the jar holds, the expired among them until the jar finds them: a request to their site, a store past a
limit, [clear_expired](clear_expired.md).

## Parameters

None.

## Return value

The number of cookies.

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
    println("{}", jar.size());
    jar.set_cookies(net::url("https://example.com/"), {net::http::cookie("a=1"), net::http::cookie("b=2")});
    println("{}", jar.size());
}
```

Output:

```text
0
2
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::net::http::cookie_jar](README.md)
