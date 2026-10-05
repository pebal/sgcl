[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::serve, async_serve

```cpp
void serve(const http::server& s, const request& r) const;                            // (1)
async::task<> async_serve(const http::server& s, const request& r) const noexcept;    // (2)
```

Runs the request through the routes of the server, as the server routes it — the pattern found and its path values
set, a redirect of a path's slash, 405 with `Allow` for a method no route of the path takes, the server's
[not_found](../server/not_found.md) or 404 — and its handler to its end, writing to this recorder's
[writer](writer.md). No connection is made: the server need not serve. What the handler throws comes out.

1. Blocks the calling thread until the handler has ended: for a thread of the program, never a worker.
2. The same for a task: `co_await rec.async_serve(s, req)`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the server whose routes take the request |
| `r` | the request, a [test_request](../test_request.md) as a rule |

## Return value

None.

## Complexity

The router's walk of the pattern, and the handler.

## Exceptions

What the handler throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server s;
    s.route("GET /books/{id}", [](net::http::request r, net::http::response_writer w) {
        w.write("book " + r.path_value("id"));
    });
    for (auto target : {"/books/42", "/films/1"}) {
        net::http::response_recorder rec;
        rec.serve(s, net::http::test_request("GET", target));
        println("{} {} {}", target, rec.status(), rec.body().size());
    }
    net::http::response_recorder wrong;
    wrong.serve(s, net::http::test_request("DELETE", "/books/42"));
    println("{} {}", wrong.status(), wrong.header("Allow"));
}
```

Output:

```text
/books/42 200 7
/films/1 404 10
405 GET, HEAD
```

## See also

- [server::route](../server/route.md): the patterns
- [sgcl::net::http::response_recorder](README.md)
