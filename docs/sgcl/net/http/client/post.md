[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](../client.md)

# sgcl::net::http::client::post, async_post

```cpp
expected<response, io::error> post(const string& url, const string& content_type,            // (1)
                                   const string& body) const;
async::task<expected<response, io::error>> async_post(const string& url,                     // (2)
                                                      const string& content_type,
                                                      const string& body) const noexcept;
```

Sends a POST of `body` to `url` with `Content-Type: content_type`, Go's `http.Post`: [send](send.md) of a request
with the field and the body set. The body is held in memory, so a 307 or a 308 sends it again and a retry on a new
connection may; it goes with its `Content-Length`. A 301, a 302 or a 303 goes on as a GET without the body.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL, `http://` or `https://` |
| `content_type` | the value of `Content-Type` |
| `body` | the body |

## Return value

The response, its body not read yet; a 4xx or a 5xx is a response. Or the error, as [send](send.md) gives it:
`net::errc::invalid_url` for a URL that does not parse or is past 512 MiB ([the limit](../../url.md#rules)).

## Complexity

That of [send](send.md), and linear in the size of the body.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

A JSON answer read into a struct of the program's, through its `describe`:

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

struct echo {
    string data;
    int64_t length = 0;

    void describe(encoding::field_list& f) {
        f.add("data", data);
        f.add("length", length);
    }
};

int main() {
    net::http::server srv;
    srv.route("POST /echo", [](net::http::request req, net::http::response_writer w)
                                -> async::task<> {
        string text = co_await req.async_text();
        w.set_header("Content-Type", "application/json");
        w.write("{\"data\": \"" + text + "\", \"length\": " + req.header("Content-Length") + "}");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.post(base + "/echo", "text/plain", "buy milk");
    echo reply = res.json<echo>();
    println("{} {} {}", res.status(), reply.data, reply.length);
    srv.close();
}
```

Output:

```text
200 buy milk 8
```

## See also

- [send](send.md): another method, a body of bytes or a stream
- [json](../response/json.md): the answer read as JSON
- [sgcl::net::http::client](../client.md)
