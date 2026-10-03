[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::header

```cpp
string header(const string& name) const noexcept;
```

Returns the value of the first field named `name` in the head of the response, found in any ASCII case, Go's
`resp.Header.Get`: `headers().get(name)` ([get](../headers/get.md)).

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
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain; charset=utf-8");
        w.write("hello\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/");
    println("{} | [{}]", res.header("content-type"), res.header("X-Missing"));
    res.close();
    srv.close();
}
```

Output:

```text
text/plain; charset=utf-8 | []
```

## See also

- [headers](headers.md): every field
- [sgcl::net::http::response](../response.md)
