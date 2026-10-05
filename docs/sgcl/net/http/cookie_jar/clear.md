[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::clear

```cpp
void clear() const noexcept;
```

Every cookie out of the jar: a user who logs out of everything, a program that starts over. The jar stays, with its
limits, for the cookies that come after.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the cookies of the jar.

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
    jar.set_cookies(net::url("https://example.com/"), {net::http::cookie("a=1"), net::http::cookie("b=2")});
    jar.clear();
    println("{} [{}]", jar.size(), jar.header(net::url("https://example.com/")));
}
```

Output:

```text
0 []
```

## See also

- [remove](remove.md): one cookie, or one domain's
- [sgcl::net::http::cookie_jar](README.md)
