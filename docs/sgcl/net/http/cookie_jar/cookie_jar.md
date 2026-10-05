[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::cookie_jar

```cpp
cookie_jar() noexcept;                             // (1)
explicit cookie_jar(const options& o) noexcept;    // (2)
cookie_jar(const cookie_jar& other) noexcept;      // (3), implicitly declared
cookie_jar(cookie_jar&& other) noexcept;           // (4), implicitly declared
```

1. An empty jar of the default limits: 180 cookies a registrable domain and 3000 in all, the limits of Chrome and
   Firefox (RFC 6265 §6.1 asks for at least 50 and 3000).
2. An empty jar of the limits of `o` ([options](../cookie_jar-options.md)); a limit of zero keeps nothing.
3. A handle of the same jar as `other`.
4. The same; `other` is still the jar, since the move of the word inside is its copy.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the limits |
| `other` | the handle whose jar this one shares |

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
    net::http::cookie_jar::options small;
    small.max_cookies_per_domain = 2;
    net::http::cookie_jar limited(small);
    net::http::cookie_jar copy = jar;
    net::url site("https://example.com/");
    for (auto& c : {"a=1", "b=2", "c=3"}) {
        copy.set_cookies(site, {net::http::cookie(c)});
        limited.set_cookies(site, {net::http::cookie(c)});
    }
    println("{} {}", jar.size(), limited.size());
}
```

Output:

```text
3 2
```

## See also

- [options](../cookie_jar-options.md): the limits
- [sgcl::net::http::cookie_jar](README.md)
