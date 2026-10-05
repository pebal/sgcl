[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::cookies

```cpp
vector<cookie> cookies(const net::url& u) const noexcept;
```

The cookies a request to `u` carries, Go's `Cookies`, in the order they are sent: the longer paths first, then the
older ones (RFC 6265 §5.4). Where Go's gives the name and the value alone, each comes with what the jar knows of it:
its `domain` (a dot in front for a domain cookie, the host alone for a host-only one, as browsers show them), `path`,
`expires` for a persistent cookie (`nullopt` for a session one; `max_age` stays `nullopt`), `secure`, `http_only`,
`partitioned` and `same_site`. Their time of last use is now.

## Parameters

| Parameter | Description |
|---|---|
| `u` | the URL of the request |

## Return value

The cookies, empty when none matches or the URL's scheme is not `http`, `https`, `ws` or `wss`.

## Complexity

Linear in the cookies of the URL's site, and the sort of those that match.

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
    jar.set_cookies(net::url("https://example.com/docs/"),
                    {net::http::cookie("theme=dark; Path=/"), net::http::cookie("page=7; Max-Age=3600"),
                     net::http::cookie("token=x; Secure; HttpOnly; SameSite=Strict; Path=/")});
    for (auto& c : jar.cookies(net::url("https://example.com/docs/intro"))) {
        println("{}={} path {} secure {} session {}", c.name, c.value, c.path, c.secure, !c.expires);
    }
    println("{}", jar.cookies(net::url("http://example.com/docs/intro")).size());
}
```

Output:

```text
page=7 path /docs secure false session false
theme=dark path / secure false session true
token=x path / secure true session true
2
```

## See also

- [header](header.md): the same as the `Cookie` field
- [all](all.md): every cookie
- [sgcl::net::http::cookie_jar](README.md)
