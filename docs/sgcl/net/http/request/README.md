[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::request

```cpp
#include "sgcl/net/http/request.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class request;
}
```

`sgcl::net::http::request` is a request of HTTP, Go's `http.Request`: one type for the [client](../client/README.md), which builds
one and sends it ([send](../client/send.md)), and for the [server](../server/README.md), which reads one and hands it to the
handler of the route it matched. A program builds a request of a method and a URL, sets its fields and its body, and
sends it; a handler reads the wildcards of its route, the query, the fields, the cookies and the body of the one it
was given.

A `request` is a handle of one word: a copy is the same request, and a request passed by value into a task keeps it
alive. Its fields are a [headers](../headers/README.md) of its own, which [headers](headers.md) gives by reference.

## Rules

- A request holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object; in a global
  or a `std` container, a [rooted](../../../core/rooted/README.md) of it ([The rules](../../../core/README.md#the-rules), 1).
- **Built by a program**: the method as given, the URL parsed at once by [net::url](../../url/README.md); one that does not parse,
  or a text past 512 MiB ([the limit](../../url/README.md#rules)), is reported by the send, `net::errc::invalid_url`, and
  [url](url.md) throws `invalid_argument` for it; the constructor throws nothing. A body
  is text, bytes, or a stream ([set_body](set_body.md)): text and bytes are held in memory and can be sent
  again (a retry, a 307); a stream is read once, with a `Content-Length` when its length is given and chunked when it
  is not.
- **Received by a server**: [url](url.md) is the URL the request was for (`http://`, the `Host` and the
  target, or the target in absolute form); [path_value](path_value.md) gives the wildcards of the route that
  matched, unescaped; [query](query.md) the first value of a name in the query; [cookie](cookie.md)
  the first cookie of a name in the `Cookie` fields; [content_length](content_length.md) the length the
  request declared (none for chunked).
- **The body of a received request** is read once, with [text](text.md), [bytes](bytes.md) or the
  stream [body](body.md); every read is bounded by the server's `max_body_bytes` (`net::errc::body_too_large`
  past it). A request without a body has no object for it: `body()` is an empty stream, one for the program. A
  handler that does not read the body has what is left read for it after it returns, up to 256 KB (Go's bound), and
  past that the connection is closed.
- **Waiting**: a handler runs on a worker, so it reads with `co_await req.async_text()`; `text()` blocks a thread (the
  reading runs on the scheduler and the thread waits) and is for code on a thread of its own.
- [stop](stop.md) is a token stopped when the server closes and when a write of the response fails (Go's
  `r.Context()`); a long handler waits on it, or checks it.
- [trailers](trailers.md) holds the trailer fields of a chunked body once the body has been read to its end.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](request.md) | constructs a request of a method and a URL, to send |
| `(destructor)` | drops the handle |
| `operator=` | makes the handle refer to another request |

#### The head

| Function | Description |
|---|---|
| [method](method.md) | the method |
| [proto](proto.md) | the protocol the request came or went over |
| [url](url.md) | the URL |
| [header](header.md) | the first value of a field |
| [headers](headers.md) | the fields |

#### Modifiers

| Function | Description |
|---|---|
| [set_header](set_header.md) | sets a field, in the place of the first of its name |
| [add_header](add_header.md) | appends a field |
| [set_body](set_body.md) | sets the body to send: text, bytes or a stream |

#### Received by a server

| Function | Description |
|---|---|
| [path_value](path_value.md) | the value of a wildcard of the route |
| [query](query.md) | the first value of a name in the query |
| [cookie](cookie.md) | the value of the first cookie of a name |
| [content_length](content_length.md) | the length of the body as the request declared it |
| [remote_endpoint](remote_endpoint.md) | the address and the port of the client |
| [stop](stop.md) | a token stopped when the server closes or a write fails |

#### The body

| Function | Description |
|---|---|
| [text, async_text](text.md) | the whole body as text |
| [bytes, async_bytes](bytes.md) | the whole body as bytes |
| [body](body.md) | the body as a stream |
| [trailers](trailers.md) | the trailer fields of a chunked body |

## Example

The server hands a request to the handler of the route it matched: the wildcards by `path_value`, the query by
`query`, the fields by `header`:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /notes/{id}", [](net::http::request req, net::http::response_writer w) {
        w.write(req.method() + " note " + req.path_value("id") + " by " + req.query("author") +
                ", " + req.header("Accept") + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request ask("GET", base + "/notes/42?author=Ann%20B");
    ask.set_header("Accept", "text/plain");
    net::http::client web;
    print("{}", web.send(ask)->text().value());
    srv.close();
}
```

Output:

```text
GET note 42 by Ann B, text/plain
```

## See also

- [client](../client/README.md), [server](../server/README.md): where a request is sent and received
- [response](../response/README.md): what a client receives back; [response_writer](../response_writer/README.md): what a handler writes
- [headers](../headers/README.md), [cookie](../cookie/README.md)
