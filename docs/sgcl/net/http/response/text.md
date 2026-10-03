[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::text, async_text

```cpp
/*(1)*/ expected<string, io::error> text() const;
/*(2)*/ async::task<expected<string, io::error>> async_text() const noexcept;
```

Reads the whole body as text, Go's `io.ReadAll(resp.Body)`, straight into the string it returns; the end of the body
gives the connection back to the client's pool. The body is read once: a second read gives what is left, `""`. A
response without a body (to HEAD, a 204, a 304) gives `""`.

1. Blocks the calling thread: the reading runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The body, or the [error](../../../io/error.md) of its reading: `io::errc::unexpected_eof` for a body cut short,
`net::errc::malformed_response` for a broken chunked framing, `ETIMEDOUT` when the client's `timeout` passes,
`std::errc::connection_reset` for an HTTP/2 stream the server reset, or the error of the connection.

## Complexity

Linear in the size of the body.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<string> fetch(string url) {
    net::http::client web;
    net::http::response res = co_await web.async_get(url);
    co_return co_await res.async_text();
}

int main() {
    net::http::server srv;
    srv.route("GET /note", [](net::http::request, net::http::response_writer w) {
        w.write("buy milk\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/note");
    print("{}", res.text().value());
    println("[{}]", res.text().value());
    print("{}", async::run(fetch(base + "/note")));
    srv.close();
}
```

Output:

```text
buy milk
[]
buy milk
```

## See also

- [bytes](bytes.md), [json](json.md), [save](save.md), [body](body.md): the body in another form
- [sgcl::net::http::response](../response.md)
