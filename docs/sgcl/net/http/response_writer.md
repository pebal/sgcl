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
- **A field value** has its CR, LF and NUL made spaces (a value from a user cannot split the response); a name that is not a token is `invalid_argument`.
- **`error(code)`** writes the status and its reason as `text/plain` (`Not Found\n`), with `X-Content-Type-Options: nosniff`, dropping what was buffered, as Go's `http.Error`; `error(code, message)` writes the message instead. **`redirect(location, code)`** sets a 3xx (302 by default) and `Location` as given.
- **`hijack()`** hands the connection over (a WebSocket, a protocol of the program's): the connection, and a reader of what is left of it, whose first bytes are the ones the server had read past this request. The server then sends nothing more on it and does not close it. Only before the head has gone (`io::errc::closed` after).
- **`async_flush()`** in a handler, which runs on a worker; `flush()` blocks a thread and is for a response written from one of the program's threads. A flush that fails (the client went away) returns the error, and the request's `stop()` is stopped.

## Members

```cpp
response_writer& set_status(int code);
int status() const noexcept;
response_writer& set_header(const string& name, const string& value);
response_writer& add_header(const string& name, const string& value);
response_writer& set_cookie(const cookie& c);            // a Set-Cookie field
http::headers& headers() const noexcept;
response_writer& write(const string& text);
response_writer& write(const slice<const byte>& data);
expected<void, io::error> flush() const;
async::task<expected<void, io::error>> async_flush() const;
void error(int code);
void error(int code, const string& message);
void redirect(const string& location, int code = status::found);
expected<pair<net::connection, io::reader>, io::error> hijack();
bool header_sent() const noexcept;
```

## See also

- [server](server.md): where a handler runs, and the example of a stream of events; [cookie](cookie.md)
