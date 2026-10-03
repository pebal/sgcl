[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::response_writer

```cpp
#include "sgcl/net/http/response_writer.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class response_writer;
}
```

`net::http::response_writer` is the response a handler of a [server](server.md) writes, Go's `http.ResponseWriter`:
the status, the fields, the body. [write](response_writer/write.md) does not wait: it adds to a buffer in memory, and
the server sends the whole response when the handler returns, with an exact Content-Length, so that a handler that
never waits is a plain function. Streaming (a large body, server-sent events) is
[async_flush](response_writer/flush.md): it sends the head and what is buffered, and the body goes on chunked from
there (to an HTTP/1.0 client, to the end of the connection). This is simplicity bought with memory: a body built
whole is held whole until it is sent.

A writer is made by the server and handed to the handler; it is a handle of one word, a `tracked_ptr` to the
response, which a copy shares. Over HTTP/2 the same writer writes a stream: a response written whole goes as HEADERS
and DATA with END_STREAM, a flush sends HEADERS and DATA without it ([server, HTTP/2](server.md#http2)).

## Rules

- A writer holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object.
- The status is 200 unless set, one of 200 to 999; after the head has gone, a new one is ignored, and so are the
  fields.
- `Date` is the server's: every response carries one, IMF-fixdate of `time::now()` (made once a second), unless the
  handler set its own.
- The framing is the server's. A Transfer-Encoding of the handler's is not sent; a Content-Length is replaced by the
  true length of a body sent whole, and honoured for a flushed one when set before the first flush (a body that then
  does not match it ends the connection after the response). `Connection: close` among the fields ends the
  connection after the response. No Content-Type is guessed (Go sniffs one): a handler that sends text says so.
- A field is kept as given and checked when the head goes: a name that is not a token or a value with CR, LF, NUL or
  another control is the writer's first error — [flush](response_writer/flush.md) returns it
  (`std::errc::invalid_argument`, the field named) and every flush after it, nothing of the head is sent, and the
  server answers 500 in its place ([server](server.md#rules)).
- A writer a task keeps past its handler writes nowhere once the server has sent the response: a write or an
  [error](response_writer/error.md) is dropped and a [flush](response_writer/flush.md) returns `io::errc::closed`;
  the connection, gone on to the next request, never sees its bytes.
- A handler runs on a worker, so it flushes with `co_await w.async_flush()`; `flush()` blocks a thread and is for a
  response written from one of the program's threads. A flush that fails (the client went away) returns the error,
  and the request's [stop](request/stop.md) is stopped.

## Member functions

#### The head

| Function | Description |
|---|---|
| [set_status](response_writer/set_status.md) | sets the status |
| [status](response_writer/status.md) | the status set |
| [set_header](response_writer/set_header.md) | sets a field, replacing the fields of its name |
| [add_header](response_writer/add_header.md) | adds a field |
| [add_cookie](response_writer/add_cookie.md) | adds a Set-Cookie field |
| [headers](response_writer/headers.md) | the fields of the response |
| [header_sent](response_writer/header_sent.md) | checks whether the head has gone |

#### The body

| Function | Description |
|---|---|
| [write](response_writer/write.md) | adds bytes, a text or a file to the body |
| [flush, async_flush](response_writer/flush.md) | sends the head and what is buffered |

#### Whole answers

| Function | Description |
|---|---|
| [error](response_writer/error.md) | an error as `text/plain`, in place of what was buffered |
| [redirect](response_writer/redirect.md) | a 3xx with `Location` |
| [hijack](response_writer/hijack.md) | hands the connection over to the program |

## Example

Three handlers: a response with its status, fields and a cookie, sent whole; an error; and a stream, whose first part
goes out at the flush. The client prints what came.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("POST /items", [](net::http::request, net::http::response_writer w) {
        net::http::cookie session("session", "abc");
        session.http_only = true;
        w.set_status(net::http::status::created)
            .set_header("Content-Type", "text/plain")
            .add_cookie(session);
        w.write("item made\n");
    });
    srv.route("GET /secret", [](net::http::request, net::http::response_writer w) {
        w.error(net::http::status::not_found);
    });
    srv.route("GET /stream", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("part 1\n");
        co_await w.async_flush();
        w.write(w.header_sent() ? "part 2, the head already sent\n" : "part 2\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    net::http::response made = web.post(base + "/items", "text/plain", "pen");
    string text = made.text();
    println("{} {} | {} | {}", made.status(), made.header("Content-Type"),
            made.header("Set-Cookie"), made.content_length().value_or(0));
    print(text);

    net::http::response hidden = web.get(base + "/secret");
    string reason = hidden.text();
    println("{} {}", hidden.status(), hidden.header("X-Content-Type-Options"));
    print(reason);

    net::http::response streamed = web.get(base + "/stream");
    string parts = streamed.text();
    println("{} {}", streamed.status(), streamed.content_length().has_value());
    print(parts);

    srv.close();
    serving.wait();
}
```

Output:

```text
201 text/plain | session=abc; HttpOnly | 10
item made
404 nosniff
Not Found
200 false
part 1
part 2, the head already sent
```

## See also

- [server](server.md): where a handler runs
- [request](request.md): the other argument of a handler
- [cookie](cookie.md): what `add_cookie` sends
