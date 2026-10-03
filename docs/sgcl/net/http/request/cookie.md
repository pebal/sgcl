[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::cookie

```cpp
string cookie(const string& name) const noexcept;
```

Returns the value of the first cookie named `name` in the `Cookie` fields of the request (`theme=dark; lang=pl`),
Go's `r.Cookie(name)`: the pairs of every `Cookie` field in their order, the name compared exactly, quotes around the
value taken off. A pair whose value holds a byte no cookie may hold is passed over.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the cookie, compared exactly |

## Return value

The value, or `""` when no cookie has the name.

## Complexity

Linear in the size of the `Cookie` fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write(req.cookie("theme") + " | " + req.cookie("lang") + " | [" + req.cookie("Theme") +
                "]\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request req("GET", base + "/");
    req.add_header("Cookie", "theme=dark; session=abc").add_header("Cookie", "lang=\"pl\"");
    net::http::client web;
    print("{}", web.send(req)->text().value());
    srv.close();
}
```

Output:

```text
dark | pl | []
```

## See also

- [cookie](../cookie/README.md): a cookie and its attributes, as `Set-Cookie` carries it
- [header](header.md): a field of the request
- [sgcl::net::http::request](README.md)
