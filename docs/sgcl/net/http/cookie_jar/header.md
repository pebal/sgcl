[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::header

```cpp
string header(const net::url& u) const noexcept;
```

The value of the `Cookie` field of a request to `u` (RFC 6265 §5.4): the [cookies](cookies.md) of the URL as
`name=value` joined by `; `, in their order, their time of last use now. A value is written as it came, in quotes
when it holds a space or a comma (the quotes [cookie::parse](../cookie/parse.md) took off), as Go's client writes it.
A [client](../client/README.md) with a jar puts this after the request's own `Cookie` pairs, in one field.

## Parameters

| Parameter | Description |
|---|---|
| `u` | the URL of the request |

## Return value

The field's value; `""` when no cookie matches.

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
    net::url site("https://shop.example.com/cart");
    jar.set_cookies(site, {net::http::cookie("lang=pl; Domain=example.com"),
                           net::http::cookie("cart=\"2 items\"; Path=/cart")});
    println("{}", jar.header(site));
    println("{}", jar.header(net::url("https://www.example.com/")));
    println("[{}]", jar.header(net::url("https://example.org/")));
}
```

Output:

```text
cart="2 items"; lang=pl
lang=pl
[]
```

## See also

- [cookies](cookies.md): the same as cookies
- [sgcl::net::http::cookie_jar](README.md)
