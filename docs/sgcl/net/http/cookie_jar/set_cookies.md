[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::set_cookies

```cpp
void set_cookies(const net::url& u, const vector<cookie>& cookies) const noexcept;
```

The cookies a response from `u` set, Go's `SetCookies`: each one, in order, stored, replaced, deleted or refused by
the rules of the jar ([Rules](README.md#rules)). A [client](../client/README.md) with a jar calls it with every
response's [cookies](../response/cookies.md); a program that receives `Set-Cookie` fields some other way reads each
with [cookie::parse](../cookie/parse.md) and hands them here.

The cookie's fields are what the jar reads: `domain` and `path` as given (a leading dot of the domain ignored, a path
that does not begin with `/` taken as none), `max_age` before `expires`, `secure`, `http_only`, `partitioned` and
`same_site`. A URL other than `http`, `https`, `ws` and `wss`, or without a host, sets nothing.

## Parameters

| Parameter | Description |
|---|---|
| `u` | the URL the response came from |
| `cookies` | the cookies it set, in the order of its fields |

## Return value

None.

## Complexity

Linear in the cookies given times the cookies of the site; a site past its limit, or the jar past its own, adds a
look over the cookies of the site or of the jar.

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
    net::url page("https://www.example.co.uk/a/b");
    // the host alone at /a; the site and under it; a public suffix, refused; a prefixed one
    jar.set_cookies(page, {net::http::cookie("id=1"), net::http::cookie("site=2; Domain=example.co.uk"),
                           net::http::cookie("all=3; Domain=co.uk"),
                           net::http::cookie("__Host-key=4; Secure; Path=/")});
    for (auto& c : jar.all()) {
        println("{} {} {}", c.name, c.domain, c.path);
    }
    jar.set_cookies(page, {net::http::cookie("site=; Max-Age=0; Domain=example.co.uk")});
    println("{}", jar.size());
}
```

Output:

```text
site .example.co.uk /a
id www.example.co.uk /a
__Host-key www.example.co.uk /
2
```

## See also

- [header](header.md), [cookies](cookies.md): what goes back
- [response::cookies](../response/cookies.md): a response's cookies
- [sgcl::net::http::cookie_jar](README.md)
