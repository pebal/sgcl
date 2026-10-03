[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::add_header

```cpp
response_writer& add_header(const string& name, const string& value) noexcept;
```

Adds the field `name: value` after the fields already there, those of the same name kept, Go's `w.Header().Add`:
the [add](../headers/add.md) of the response's [headers](headers.md), for a field that may come more than once
(`Vary`, `Link`). The field is checked when the head goes, as [set_header](set_header.md)'s is; after the head has
gone, it goes nowhere.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field |
| `value` | its value |

## Return value

`*this`.

## Complexity

Constant, amortized.

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
        w.add_header("Link", "</style.css>; rel=preload")
            .add_header("Link", "</app.js>; rel=preload");
        w.write("page\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/");
    string text = res.text();
    for (auto& link : res.headers().get_all("Link")) {
        println("{}", link);
    }
    srv.shutdown();
}
```

Output:

```text
</style.css>; rel=preload
</app.js>; rel=preload
```

## See also

- [set_header](set_header.md): a field in place of the others of its name
- [add_cookie](add_cookie.md): a Set-Cookie field
- [sgcl::net::http::response_writer](README.md)
