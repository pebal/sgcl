[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [server](../server.md)

# sgcl::net::http::server::access_log

```cpp
/*(1)*/ server& access_log(const slog::logger& log) noexcept;
/*(2)*/ server& access_log();
```

Writes a record of every exchange, when its response is finished, at `info`, and at `error` for a 5xx. The record's
message is `request` and its attributes `method`, `path`, `proto`, `status`, `bytes` (of the body: 0 for HEAD, 204,
304), `duration` (from the request's first byte; over HTTP/2 from the handler's start), `remote`, `user_agent` (`""`
for none), and `request_id` when the request has an `X-Request-ID`. Nothing managed per request: the attributes are
views of the request, the remote address is written into the line. A request refused before its handler (400, 413,
417, 431 over HTTP/1.1) and a hijacked connection are not logged.

1. Through `log`, as it is given: `slog::options{.out = file, .json = true, .buffered = true}` for JSON lines in
   batches ([slog::logger](../../../slog/logger.md)).
2. Through the [default logger](../../../slog/logger.md), buffered: a batch per worker.

Like the settings, the logger is the handle's own, not shared with its copies, and read when [serve](serve.md) is
called.

## Parameters

| Parameter | Description |
|---|---|
| `log` | the logger the records go through |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello\n");
    });
    srv.access_log(slog::logger(io::stdout));
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    for (auto path : {"/hello", "/nothing"}) {
        net::http::response res = web.get(base + path);
        res.text();
    }
    srv.shutdown();
}
```

Output:

```text
time=2026-10-02T12:00:00.000+02:00 level=INFO msg=request method=GET path=/hello proto=HTTP/1.1 status=200 bytes=6 duration=84.5µs remote=127.0.0.1:52811 user_agent=""
time=2026-10-02T12:00:00.000+02:00 level=INFO msg=request method=GET path=/nothing proto=HTTP/1.1 status=404 bytes=10 duration=21.3µs remote=127.0.0.1:52811 user_agent=""
```

## See also

- [slog](../../../slog/README.md): the loggers, text and JSON
- [sgcl::net::http::server](../server.md)
