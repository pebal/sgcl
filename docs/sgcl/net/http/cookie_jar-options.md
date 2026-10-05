[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [cookie_jar](cookie_jar/README.md) › options

# sgcl::net::http::cookie_jar::options

```cpp
#include "sgcl/net/http/cookie_jar.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cookie_jar {
    public:
        struct options {
            size_t max_cookies = 3000;
            size_t max_cookies_per_domain = 180;
        };
    };
}
```

`sgcl::net::http::cookie_jar::options` is the limits of a [cookie_jar](cookie_jar/README.md), given to its
[constructor](cookie_jar/cookie_jar.md): a plain struct, its fields set by name. RFC 6265 §6.1 asks a jar for at
least 50 cookies a domain and 3000 in all; the defaults are Chrome's and Firefox's. A cookie stored past a limit
makes room by RFC 6265bis §5.7's order: the expired first, then within the domain those without `Secure`, the least
recently used first in each.

## Member objects

| Member | Description |
|---|---|
| `max_cookies` | the most cookies of the jar; 3000 by default |
| `max_cookies_per_domain` | the most cookies of one registrable domain (`example.co.uk`, and every host under it); 180 by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::cookie_jar::options o;
    o.max_cookies_per_domain = 2;
    net::http::cookie_jar jar(o);
    net::url site("https://example.com/");
    jar.set_cookies(site, {net::http::cookie("a=1; Secure"), net::http::cookie("b=2")});
    jar.set_cookies(site, {net::http::cookie("c=3")});
    println("{}", jar.header(site));
}
```

Output:

```text
a=1; c=3
```

## See also

- [cookie_jar](cookie_jar/README.md)
- [sgcl::net::http](README.md)
