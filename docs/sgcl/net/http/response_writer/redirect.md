[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::redirect

```cpp
void redirect(const string& location, int code = status::found);
```

Answers with a redirect, Go's `http.Redirect`: sets the status, 302 unless given, and the field `Location` to
`location` as given. The body is what the handler writes, none unless it does.

## Parameters

| Parameter | Description |
|---|---|
| `location` | the target, absolute or relative to the request's URL |
| `code` | a 3xx status: [status](../status.md)`::found` (302) by default, `moved_permanently`, `see_other`, `temporary_redirect`, `permanent_redirect` |

## Return value

None.

## Complexity

Linear in the number of fields.

## Exceptions

`invalid_argument` when `code` is not a 3xx, before anything changes.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /old", [](net::http::request, net::http::response_writer w) {
        w.redirect("/new", net::http::status::moved_permanently);
    });
    srv.route("GET /new", [](net::http::request, net::http::response_writer w) {
        w.write("the new page\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/old");
    string text = res.text();
    print("{} {} {}", res.status(), res.url().path(), text);
    srv.shutdown();
}
```

Output:

```text
200 /new the new page
```

## See also

- [client](../client/README.md): follows a redirect
- [sgcl::net::http::response_writer](README.md)
