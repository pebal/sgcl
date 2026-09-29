# sgcl::net::http::response_writer

```cpp
#include "sgcl/net/http/response_writer.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class response_writer;   // the response a handler writes (Go's ResponseWriter); a handle of one word
}
```

`write` does not wait: it adds to a buffer in memory, and the server sends the whole response when the handler returns, with an exact Content-Length. A handler that never waits is therefore a plain function. Streaming (a large body, server-sent events) is `async_flush()`: it sends the head and what is buffered, and the body goes on chunked from there (to an HTTP/1.0 client, to the end of the connection). This is simplicity bought with memory: a body built whole is held whole until it is sent.

## Rules

- **The status** is 200 unless set, one of 200 to 999 (`invalid_argument` otherwise: an informational status is not the handler's); after the head has gone, a new one is ignored.
- **`Date`** is the server's too: every response carries one, IMF-fixdate of `time::now()` (made once a second), unless the handler set its own.
- **The framing is the server's.** A Transfer-Encoding of the handler's is not sent; a Content-Length is replaced by the true length of a body sent whole, and honoured for a flushed one (a body that then does not match it ends the connection after the response). `Connection: close` among the fields ends the connection after the response. No Content-Type is guessed (Go sniffs one): a handler that sends text says so.
- **A field** is kept as given and checked when the head goes: a name that is not a token or a value with CR, LF, NUL or another control is the writer's first error — `flush` returns it (`std::errc::invalid_argument`, the field named) and every flush after it, nothing of the head is sent, and the server answers 500 in its place ([server](server.md)).
- **`error(code)`** writes the status and its reason as `text/plain` (`Not Found\n`), with `X-Content-Type-Options: nosniff`, dropping what was buffered, as Go's `http.Error`; `error(code, message)` writes the message instead. **`redirect(location, code)`** sets a 3xx (302 by default) and `Location` as given.
- **`hijack()`** hands the connection over (a WebSocket, a protocol of the program's): the connection, and a reader of what is left of it, whose first bytes are the ones the server had read past this request. The server then sends nothing more on it and does not close it. Only before the head has gone (`io::errc::closed` after). Over HTTP/2 a response is a stream, not a connection: `hijack()` gives `std::errc::operation_not_supported` (Go has no Hijacker there), and a flush sends HEADERS and DATA without END_STREAM ([server, HTTP/2](server.md#http2)).
- **`write(file)`** makes the file, from its position to its end, the body's bytes, and moves the position to the end. When the file is all of the body (HTTP/1.1, nothing written before or after it, no flush), its length is the Content-Length and it goes after the head by `sendfile`: the file's pages go to the socket without a copy through the process. Over TLS it is read in blocks that are sealed where they lie. In any other case (bytes around it, a flush, HTTP/2) its bytes are taken into the body as `write(bytes)` takes them. It writes the body only: Content-Type and the other fields stay the handler's. A file served whole is one line, `w.write(io::open(path))`.
- **`async_flush()`** in a handler, which runs on a worker; `flush()` blocks a thread and is for a response written from one of the program's threads. A flush that fails (the client went away) returns the error, and the request's `stop()` is stopped.

## Members

### The head

```cpp
response_writer& set_status(int code);
int status() const noexcept;
response_writer& set_header(const string& name, const string& value);
response_writer& add_header(const string& name, const string& value);
response_writer& add_cookie(const cookie& c);            // a Set-Cookie field added: one per cookie
http::headers& headers() const noexcept;
bool header_sent() const noexcept;
```

The status (200 unless set) and the fields of the response, kept until the head goes; `header_sent()` tells whether it has, after which neither changes anything.

### The body

```cpp
response_writer& write(const string& text);
response_writer& write(const slice<const byte>& data);
response_writer& write(const io::file& f);                // the file from its position to its end; alone, sent by sendfile after the head
expected<void, io::error> flush() const;
async::task<expected<void, io::error>> async_flush() const;
```

`write` adds to the buffered body and never waits. A flush sends the head and what is buffered, and the body goes on chunked; `async_flush` is a handler's, `flush` a thread's.

### Whole answers

```cpp
void error(int code);
void error(int code, const string& message);
void redirect(const string& location, int code = status::found);
```

An error as `text/plain`, its reason or the message given, in place of what was buffered; a redirect with `Location`, 302 unless told.

### hijack

```cpp
expected<pair<net::connection, io::reader>, io::error> hijack();
```

The connection handed over to the program, with a reader of what the server had read past this request; before the head has gone, and not over HTTP/2.

## Example

Three handlers: a response with its status, fields and a cookie, sent whole; an error; and a stream, whose first part goes out at the flush. The client prints what came.

```cpp
#include "sgcl/async/async.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"

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

- [server](server.md): where a handler runs; [cookie](cookie.md): what `add_cookie` sends
