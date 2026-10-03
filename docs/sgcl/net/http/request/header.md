[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::header

```cpp
string header(const string& name) const noexcept;
```

Returns the value of the first field named `name`, found in any ASCII case, Go's `r.Header.Get`: `headers().get(name)`
([get](../headers/get.md)). On the server the fields are those of the head as it came; of a request the program
built, those it set.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field, in any case |

## Return value

The value, or `""` when there is no field of the name.

## Complexity

Linear in the number of fields.

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
        w.write("[" + req.header("user-agent") + "] [" + req.header("X-Missing") + "]\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request req("GET", base + "/");
    req.set_header("User-Agent", "notes/1.0");
    net::http::client web;
    print("{}", web.send(req)->text().value());
    srv.close();
}
```

Output:

```text
[notes/1.0] []
```

## See also

- [headers](headers.md): every field
- [sgcl::net::http::request](../request.md)
