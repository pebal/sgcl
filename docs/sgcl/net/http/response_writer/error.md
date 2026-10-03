[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](../response_writer.md)

# sgcl::net::http::response_writer::error

```cpp
void error(int code);                           // (1)
void error(int code, const string& message);    // (2)
```

Answers with an error, Go's `http.Error`: sets the status and writes a line of text as the body, in place of what was
buffered, with `Content-Type: text/plain; charset=utf-8` and `X-Content-Type-Options: nosniff`; a Content-Length the
handler set is dropped.

1. The line is the status's [reason](../reason.md): `Not Found` for 404.
2. The line is `message`.

After the head has gone, the status and the fields stay as sent and the line is added to the body; after the
response has ended, nothing is.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the status, 200 to 999: a constant of [status](../status.md) or a number |
| `message` | the text of the body, without its line feed |

## Return value

None.

## Complexity

Linear in the size of the message.

## Exceptions

`invalid_argument` when `code` is not 200 to 999, before anything changes.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /secret", [](net::http::request, net::http::response_writer w) {
        w.write("half of a page");
        w.error(net::http::status::forbidden);
    });
    srv.route("GET /busy", [](net::http::request, net::http::response_writer w) {
        w.error(net::http::status::service_unavailable, "try again in a minute");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    for (auto path : {"/secret", "/busy"}) {
        net::http::response res = web.get(base + path);
        string text = res.text();
        print("{} {} | {}", res.status(), res.header("Content-Type"), text);
    }
    srv.shutdown();
}
```

Output:

```text
403 text/plain; charset=utf-8 | Forbidden
503 text/plain; charset=utf-8 | try again in a minute
```

## See also

- [reason](../reason.md): the reason of a status
- [set_status](set_status.md): the status alone
- [sgcl::net::http::response_writer](../response_writer.md)
