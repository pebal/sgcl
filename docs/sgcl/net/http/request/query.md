[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::query

```cpp
string query(const string& name) const noexcept;
```

Returns the first value of `name` in the query of the URL, decoded (`%20` and `+` a space), Go's
`r.URL.Query().Get(name)`. The value is found in the query's text and only it is decoded: the pairs before it are not
made into strings, nor is a list of them made, as [query_params::first](../../query_params/first.md) does it; the
query of a URL is within the limit `first` keeps, so nothing is refused. Every value of a name, or every pair, is `url().query_params()`
([url::query_params](../../url/query_params.md)).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, as it stands in the query once decoded |

## Return value

The first value of the name, decoded, or `""` when the query has none.

## Complexity

Linear in the size of the query.

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
    srv.route("GET /search", [](net::http::request req, net::http::response_writer w) {
        w.write(req.query("q") + " | " + req.query("tag") + " | [" + req.query("page") + "]\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    print("{}", web.get(base + "/search?q=fresh+milk&tag=a%26b&tag=c")->text().value());
    srv.close();
}
```

Output:

```text
fresh milk | a&b | []
```

## See also

- [path_value](path_value.md): a wildcard of the route
- [url](url.md): the whole URL
- [sgcl::net::http::request](../request.md)
