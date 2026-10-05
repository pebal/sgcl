[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::cookies

```cpp
vector<http::cookie> cookies() const noexcept;
```

The cookies of the response's `Set-Cookie` fields, in their order, each read by [cookie::parse](../cookie/parse.md),
Go's `Response.Cookies`; a field that holds no cookie is passed over. A [client](../client/README.md) with a
[cookie_jar](../cookie_jar/README.md) puts them in the jar by itself, every redirect's too.

## Parameters

None.

## Return value

The cookies; empty when the response sets none.

## Complexity

Linear in the fields of the head.

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
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.add_cookie(net::http::cookie("id=7; Max-Age=60"));
        w.add_cookie(net::http::cookie("theme=dark; Path=/docs"));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/");
    for (auto& c : res.cookies()) {
        println("{}={} {} {}", c.name, c.value, c.path, c.max_age.has_value());
    }
    srv.close();
}
```

Output:

```text
id=7  true
theme=dark /docs false
```

## See also

- [headers](headers.md): the fields themselves
- [cookie_jar](../cookie_jar/README.md): where a client keeps them
- [sgcl::net::http::response](README.md)
