[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](../client.md)

# sgcl::net::http::client::get, async_get

```cpp
/*(1)*/ expected<response, io::error> get(const string& url) const;
/*(2)*/ async::task<expected<response, io::error>> async_get(const string& url) const noexcept;
```

Sends a GET of `url` with no fields of the program's and no body, Go's `http.Get` and `Client.Get`: [send](send.md) of
`request("GET", url)`, the redirects followed.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL, `http://` or `https://` |

## Return value

The response, its body not read yet; a 4xx or a 5xx is a response. Or the error, as [send](send.md) gives it:
`net::errc::invalid_url` for a URL that does not parse or is past 512 MiB ([the limit](../../url.md#rules)).

## Complexity

That of [send](send.md).

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

In a task, the response and its body are awaited:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<int> fetch(string url) {
    net::http::client web;
    net::http::response res = co_await web.async_get(url);
    string text = co_await res.async_text();
    println("{}, {} bytes", res.status(), text.size());
    co_return 0;
}

int main() {
    net::http::server srv;
    srv.route("GET /license", [](net::http::request, net::http::response_writer w) {
        w.write(string("Apache License, Version 2.0\n").repeat(100));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    int code = async::run(fetch(base + "/license"));
    srv.close();
    return code;
}
```

Output:

```text
200, 2800 bytes
```

## See also

- [head](head.md): the head alone; [download](download.md): the body into a file
- [send](send.md): a request built by hand
- [sgcl::net::http::client](../client.md)
