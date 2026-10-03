[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](../response_writer.md)

# sgcl::net::http::response_writer::set_status

```cpp
response_writer& set_status(int code);
```

Sets the status of the response, Go's `WriteHeader` without the sending: the head goes when the handler returns or
at the first [flush](flush.md). The status is 200 unless set. An informational status (100 to 199) is not the
handler's: the server sends `100 Continue` itself ([server](../server.md#the-head)). After the head has gone, the call
is ignored.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the status, 200 to 999: a constant of [status](../status.md) or a number |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

`invalid_argument` when `code` is not 200 to 999; the status is left as it was.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("POST /items", [](net::http::request, net::http::response_writer w) {
        w.set_status(net::http::status::created);
        w.write("made\n");
    });
    srv.route("GET /early", [](net::http::request, net::http::response_writer w) {
        try {
            w.set_status(net::http::status::continue_);
        } catch (const invalid_argument& e) {
            w.write(string(e.what()) + "\n");
        }
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    net::http::response made = web.post(base + "/items", "text/plain", "pen");
    string text = made.text();
    print("{} {}", made.status(), text);
    net::http::response early = web.get(base + "/early");
    string refused = early.text();
    print("{} {}", early.status(), refused);
    srv.shutdown();
}
```

Output:

```text
201 made
200 http::response_writer: a status is 200 to 999
```

## See also

- [status](status.md): the status set
- [error](error.md), [redirect](redirect.md): a status with its body or its `Location`
- [sgcl::net::http::response_writer](../response_writer.md)
