[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::path_value

```cpp
string path_value(const string& name) const noexcept;
```

Returns the value of the wildcard `{name}` of the route that matched the request, unescaped, Go's `r.PathValue`. A
wildcard of the whole rest, `{name...}`, gives the rest of the path. A name the pattern does not have, and any name of
a request the program built, gives `""`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the wildcard, without the braces |

## Return value

The value, unescaped, or `""` when the route has no wildcard of the name.

## Complexity

Linear in the number of wildcards of the route.

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
    srv.route("GET /users/{user}/files/{path...}", [](net::http::request req,
                                                      net::http::response_writer w) {
        w.write(req.path_value("user") + " | " + req.path_value("path") + " | [" +
                req.path_value("other") + "]\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    print("{}", web.get(base + "/users/Ann%20B/files/notes/2024/may.txt")->text().value());
    srv.close();
}
```

Output:

```text
Ann B | notes/2024/may.txt | []
```

## See also

- [route](../server/route.md): the patterns and their wildcards
- [query](query.md): a value of the query
- [sgcl::net::http::request](../request.md)
