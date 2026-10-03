[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](README.md)

# sgcl::net::http::client::head, async_head

```cpp
expected<response, io::error> head(const string& url) const;                                // (1)
async::task<expected<response, io::error>> async_head(const string& url) const noexcept;    // (2)
```

Sends a HEAD of `url`, Go's `http.Head`: [send](send.md) of `request("HEAD", url)`. The response is the head a GET
would have, with no body: its fields and its declared length are there, and its connection goes back to the pool at
once. A redirect of any status goes on as HEAD.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL, `http://` or `https://` |

## Return value

The response, without a body; a 4xx or a 5xx is a response. Or the error, as [send](send.md) gives it:
`net::errc::invalid_url` for a URL that does not parse or is past 512 MiB ([the limit](../../url/README.md#rules)).

## Complexity

That of [send](send.md).

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

A route of `GET` answers a HEAD too, and the server sends the head alone:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /license", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write(string("Apache License, Version 2.0\n").repeat(100));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.head(base + "/license");
    println("{} {} {}", res.status(), res.header("Content-Type"), res.content_length().value());
    println("{} bytes of body", res.text()->size());
    srv.close();
}
```

Output:

```text
200 text/plain 2800
0 bytes of body
```

## See also

- [get](get.md): the head and the body
- [sgcl::net::http::client](README.md)
