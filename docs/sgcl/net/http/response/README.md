[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::response

```cpp
#include "sgcl/net/http/response.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class response;
}
```

`sgcl::net::http::response` is a response as the [client](../client/README.md) returns it, Go's `http.Response`: the status and
the head read, the body still on the connection. A 4xx or a 5xx is a response and not an error, as in Go:
[ok](ok.md) tells a 2xx. The body is read once, as text, bytes, JSON, into a file or as a stream, and its end
gives the connection back to the client's pool, with no close; Go asks for `resp.Body.Close()` too.

A `response` is a handle of one word: a copy is the same response, and a response passed by value into a task keeps
it, and its connection, alive. The program does not make one: the client does.

## Rules

- A response holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object; in a global
  or a `std` container, a [rooted](../../../core/rooted/README.md) of it ([The rules](../../../core/README.md#the-rules), 1).
- **The body** is read once, with [text](text.md), [bytes](bytes.md), [json](json.md),
  [save](save.md) or the stream [body](body.md), and its end gives the connection back to the
  client's pool. In a task, `co_await res.async_text()`; `text()` blocks the thread.
- **[close](close.md)** gives the body up without waiting: when the rest of it is already in the connection's
  buffer it is dropped and the connection goes back to the pool, otherwise the connection is closed. A response
  neither read nor closed keeps its connection out of the pool until the collector finds it. The stream `body()` has
  no close of its own: the body is given up by the response's `close()`, not by Go's `resp.Body.Close()`.
- A body cut short is `io::errc::unexpected_eof`, a chunked framing broken `net::errc::malformed_response`, a total
  `timeout` of the client passing while it is read `ETIMEDOUT`.
- **Over HTTP/2** ([proto](proto.md) is `"HTTP/2.0"`) the body is the stream's DATA, its window given back to
  the server as it is read. `close()` of a body not read to its end resets the stream (`CANCEL`) and leaves the
  connection to the other requests. A stream the server resets while its body is read is
  `std::errc::connection_reset`.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the copy and the move constructors: the same response; a response is made by the client |
| `(destructor)` | drops the handle |
| `operator=` | makes the handle refer to another response |

#### The head

| Function | Description |
|---|---|
| [status](status.md) | the status code |
| [ok](ok.md) | checks whether the status is 2xx |
| [proto](proto.md) | the protocol the response came over |
| [header](header.md) | the first value of a field |
| [headers](headers.md) | the fields |
| [content_length](content_length.md) | the length of the body as the response declared it |
| [url](url.md) | the URL the response came from, the last of the redirects |

#### The body

| Function | Description |
|---|---|
| [text, async_text](text.md) | the whole body as text |
| [bytes, async_bytes](bytes.md) | the whole body as bytes |
| [json, async_json](json.md) | the body read as JSON: a value, or a struct of the program's |
| [save, async_save](save.md) | the body streamed into a file |
| [body](body.md) | the body as a stream |
| [trailers](trailers.md) | the trailer fields of a chunked body |
| [close](close.md) | gives the body up |

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
        w.redirect("/new");
    });
    srv.route("GET /new", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("moved here\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/old");
    println("{} {} {} {}", res.status(), res.ok(), res.url().path(), res.header("Content-Type"));
    print("{}", res.text().value());
    srv.close();
}
```

Output:

```text
200 true /new text/plain
moved here
```

## See also

- [client](../client/README.md): where it comes from; [request](../request/README.md): what was sent
- [headers](../headers/README.md); [cookie](../cookie/README.md): `cookie::parse` of a `Set-Cookie` field
- [status](../status.md): the codes as constants
