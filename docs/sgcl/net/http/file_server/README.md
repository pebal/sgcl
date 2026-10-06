[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::file_server

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class file_server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The files of a directory as a handler of a [server](../server/README.md), Go's `http.FileServer` (with
`http.StripPrefix` in the route's wildcard): the handler [serve](../serve.md) runs, for a server of the program's that
does more than serve files, or that stops. On a route with a `{path...}` wildcard the wildcard's value is the name
under the directory (`GET /static/{path...}` serves `/static/a.css` as `a.css`); on a route without one, the URL's
path is.

Each name goes through [io::path::under](../../../io/path/under.md), so that none reaches a file outside the
directory; a directory is its `index.html`, the URL of a directory without its slash redirected to the one with it;
a file is answered as [serve_file](../serve_file.md) answers it: the validators, the conditional requests, the
ranges, the body by `sendfile`.

## Rules

- A value: the directory's name and the options, copied with it. It holds no state of its own beyond them; the strong
  ETags' digests are kept for the process.
- The handler never waits: it is a plain function of the server, its reads and `stat`s done on the worker.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](file_server.md) | the directory and the options |
| [operator()](operator_call.md) | the handler: a request answered with a file of the directory |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("public/css");
    (void)io::write_file("public/css/site.css", "body { margin: 0 }\n");
    net::http::server srv;
    srv.route("GET /static/{path...}", net::http::file_server("public"));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("home\n"); });
    auto served = srv.serve(":8080");
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl http://localhost:8080/static/css/site.css
body { margin: 0 }
$ curl http://localhost:8080/
home
```

## See also

- [serve](../serve.md): the same in one call, with no server of the program's
- [serve_file](../serve_file.md): one file
- [serve_options](../serve_options.md): the ETag, the ranges
- [sgcl::net::http](../README.md)
