[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](../server.md)

# sgcl::net::http::server::route

```cpp
template<class Handler>
server& route(const string& pattern, Handler handler);
```

Registers `handler` for the requests `pattern` matches, as Go 1.22's `ServeMux.HandleFunc`. Routes are registered
before [serve](serve.md); the copies of the server share them.

A pattern is `"[METHOD ][HOST]/PATH"`. A segment of the path is a literal, `{name}` (one segment, not empty),
`{name...}` (the rest of the path, last) or `{$}` (the end of a path ending in `/`, last); a path ending in `/`
matches everything under it. `GET` matches HEAD too. Of the patterns that match a request the most specific wins,
except that a pattern with a host wins over one without. The table is held to Go's own `ServeMux`
(`tools/route_oracle.go`), conflicts included.

What no pattern matches as it stands: a path with an empty segment inside is redirected (307) to the one without; a
subtree named without its slash (`/images` for `/images/`) is redirected there (307), as Go does; a path some pattern
matches for other methods is 405 with `Allow`; anything else goes to [not_found](not_found.md) (404 `text/plain` by
default). The path is the one the URL parser normalized (`.` and `..` resolved, [url](../../url.md)), and each
segment is unescaped before it is compared: `{id}` of `/posts/a%20b` is `a b`, read by
[request::path_value](../request/path_value.md).

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the method, the host and the path the handler serves |
| `handler` | a function of `(request, response_writer)` that returns `void` or `async::task<>`; another return type does not compile |

## Return value

`*this`.

## Complexity

Linear in the number of routes: the new pattern is compared with each for a conflict.

## Exceptions

`invalid_argument` for a pattern that is not one (no path, a method that is not a token, an empty segment, a
wildcard that is not a whole segment or whose name is not an identifier, a name used twice, a segment after
`{name...}` or `{$}`), and for one that conflicts with a pattern already registered: both match some request and
neither is more specific. That is a broken program, a panic in Go. What the move of `handler` throws. The routes are
left as they were.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /files/", [](net::http::request req, net::http::response_writer w) {
        w.write("under files: " + req.url().path() + "\n");
    });
    srv.route("GET /files/{name}", [](net::http::request req, net::http::response_writer w) {
        w.write("one file: " + req.path_value("name") + "\n");
    });
    srv.route("GET /files/{$}", [](net::http::request, net::http::response_writer w) {
        w.write("the listing\n");
    });
    try {
        srv.route("GET /files/{other}", [](net::http::request, net::http::response_writer) {});
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }

    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());
    net::http::client web;
    for (auto path : {"/files/a%20b", "/files/a/b", "/files/", "/files"}) {
        net::http::response res = web.get(base + path);
        string text = res.text();
        print(text);
    }
    srv.shutdown();
}
```

Output:

```text
http::server: the pattern "GET /files/{other}" conflicts with "GET /files/{name}"
one file: a b
under files: /files/a/b
the listing
the listing
```

## See also

- [not_found](not_found.md): what no route matches
- [request::path_value](../request/path_value.md): the values of the wildcards
- [sgcl::net::http::server](../server.md)
