[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::add_header

```cpp
request& add_header(const string& name, const string& value) noexcept;
```

Appends a field `name: value`, Go's `r.Header.Add`: `headers().add(name, value)` ([add](../headers/add.md)), the
fields of the same name already there kept before it. The name and the value are checked by the send, as for
[set_header](set_header.md).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field |
| `value` | the value |

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
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write(to_string(req.headers().get_all("Accept").size()) + " Accept fields\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request req("GET", base + "/");
    req.add_header("Accept", "text/html").add_header("Accept", "text/plain");
    net::http::client web;
    print("{}", web.send(req)->text().value());
    srv.close();
}
```

Output:

```text
2 Accept fields
```

## See also

- [set_header](set_header.md): one field of the name
- [sgcl::net::http::request](README.md)
