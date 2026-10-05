[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::remove

```cpp
bool remove(const string& domain, const string& path, const string& name) const noexcept;    // (1)
size_t remove(const string& domain) const noexcept;                                          // (2)
```

1. The cookie of `name` and `path` set for `domain`: a host-only cookie of that host and a domain cookie of that
   domain both, a dot in front of `domain` ignored, its case and IDNA as the jar compares hosts.
2. Every cookie of `domain` and of the names under it: `"example.com"` takes `www.example.com`'s too, as a browser's
   "remove the site's data" does.

## Parameters

| Parameter | Description |
|---|---|
| `domain` | a host or a domain, `"example.com"` or `".example.com"` |
| `path` | the cookie's path, as [all](all.md) shows it |
| `name` | the cookie's name |

## Return value

- (1) `true` when a cookie was there.
- (2) The number of cookies removed.

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
    jar.set_cookies(net::url("https://www.example.com/"),
                    {net::http::cookie("a=1"), net::http::cookie("a=2; Domain=example.com"),
                     net::http::cookie("b=3")});
    jar.set_cookies(net::url("https://example.org/"), {net::http::cookie("c=4")});
    println("{}", jar.remove("www.example.com", "/", "a"));
    println("{}", jar.remove("www.example.com", "/", "a"));
    println("{}", jar.header(net::url("https://www.example.com/")));
    println("{} {}", jar.remove("example.com"), jar.size());
}
```

Output:

```text
true
false
a=2; b=3
2 1
```

## See also

- [clear](clear.md): every cookie
- [sgcl::net::http::cookie_jar](README.md)
