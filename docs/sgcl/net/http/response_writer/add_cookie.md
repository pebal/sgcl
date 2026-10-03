[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::add_cookie

```cpp
response_writer& add_cookie(const cookie& c);
```

Adds a `Set-Cookie` field of the cookie, as its [to_string](../cookie/to_string.md) writes it, Go's `http.SetCookie`:
one field per cookie, so several cookies are several fields. After the head has gone, the field goes nowhere.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the cookie to send |

## Return value

`*this`.

## Complexity

Linear in the size of the cookie's text.

## Exceptions

What [cookie::to_string](../cookie/to_string.md) throws: `invalid_argument` for a name that is not a token of RFC
6265. No field is added then.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /login", [](net::http::request, net::http::response_writer w) {
        net::http::cookie session("session", "abc");
        session.http_only = true;
        session.path = "/";
        w.add_cookie(session).add_cookie(net::http::cookie("theme", "dark"));
        w.write("in\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/login");
    string text = res.text();
    for (auto& field : res.headers().get_all("Set-Cookie")) {
        println("{}", field);
    }
    srv.shutdown();
}
```

Output:

```text
session=abc; Path=/; HttpOnly
theme=dark
```

## See also

- [cookie](../cookie/README.md): the fields of a cookie, written and read
- [request::cookie](../request/cookie.md): a cookie the client sent back
- [sgcl::net::http::response_writer](README.md)
