[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::url

```cpp
net::url url() const;
```

Returns the URL of the request, Go's `r.URL`, as a [net::url](../../url/README.md). Of a request the program built, the URL
it gave, as parsed. Of a request a server received, the URL it was for: `http://`, the `Host` field and the target,
or the target when it came in absolute form; a request without `Host` is taken as for `localhost`.

## Parameters

None.

## Return value

The URL.

## Complexity

Constant; on the server, the first call may parse the URL from the head.

## Exceptions

`invalid_argument` when the URL given to the [constructor](request.md) did not parse; the send reports it first, as
`net::errc::invalid_url`.

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
        net::url u = req.url();
        w.write(u.scheme() + " " + u.path() + " " + u.query() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    print("{}", web.get(base + "/search?q=milk&page=2")->text().value());

    net::http::request broken("GET", "no URL at all");
    try {
        println("{}", broken.url());
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
    srv.close();
}
```

Output:

```text
http /search q=milk&page=2
http::request: the URL does not parse
```

## See also

- [query](query.md): a value of the query
- [response::url](../response/url.md): where the redirects of a response ended
- [sgcl::net::http::request](README.md)
