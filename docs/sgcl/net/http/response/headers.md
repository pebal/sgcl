[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::headers

```cpp
const http::headers& headers() const noexcept;
```

Returns the fields of the head of the response, Go's `resp.Header`: the [headers](../headers/README.md) as they came, in
their order, the names as written (over HTTP/2, in lower case). They are slices of the one string the head was copied
into. The reference is valid while the response is.

## Parameters

None.

## Return value

A reference to the fields.

## Complexity

Constant.

## Exceptions

None.

## Example

The `Set-Cookie` fields of a response, each read as a cookie:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /login", [](net::http::request, net::http::response_writer w) {
        w.add_cookie(net::http::cookie("session", "abc"));
        w.add_cookie(net::http::cookie("theme", "dark"));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/login");
    for (const string& field : res.headers().get_all("Set-Cookie")) {
        net::http::cookie c = net::http::cookie::parse(field).value();
        println("{} = {}", c.name, c.value);
    }
    res.close();
    srv.close();
}
```

Output:

```text
session = abc
theme = dark
```

## See also

- [header](header.md): the first value of a name
- [sgcl::net::http::headers](../headers/README.md)
- [sgcl::net::http::response](README.md)
