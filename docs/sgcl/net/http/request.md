# sgcl::net::http::request

```cpp
#include "sgcl/net/http/request.h"   // or "sgcl/net/http/http.h", "sgcl/sgcl.h"

namespace sgcl::net::http {
    class request;   // one type for the client, which builds it, and the server, which hands it to a handler
}
```

A request, as in Go: the client builds one and sends it ([`client::send`](client.md)), the server reads one and hands it to a handler ([server](server.md)). A handle of one word: a copy is the same request.

## Rules

- **Built by a program**: the method as given, the URL parsed at once by [`net::url`](../url.md) (one that does not parse is reported by the send, `invalid_url`, and `url()` throws `invalid_argument` for it). A body is text, bytes, or a stream: text and bytes are held in memory and can be sent again (a retry, a 307); a stream is read once, with a Content-Length when its length is given and chunked when it is not.
- **Received by a server**: `url()` is the URL the request was for, `http://` and the Host and the target (or the target in absolute form); `path_value` gives the wildcards of the route that matched, unescaped; `query` the first value of a name in the query; `cookie` the first cookie of a name in the Cookie fields; `content_length` the length the request declared (none for chunked).
- **The body of a received request** is read once, with `text()`, `bytes()` or the stream `body()`; every read is bounded by the server's `max_body_bytes` (`body_too_large` past it). A request without a body has no object for it: `body()` is an empty stream, one for the program. A handler runs on a worker, so it reads with `co_await req.async_text()`; `text()` blocks a thread (the reading runs on the scheduler and the thread waits) and is for code on a thread of its own.
- **`stop()`** is a token stopped when the server closes (`close()`) and when a write of the response fails; a long handler waits on it, or checks it.
- `trailers()` holds the trailer fields of a chunked body once the body has been read to its end.

## Members

```cpp
request(const string& method, const string& url);

string method() const;
string proto() const;                    // "HTTP/1.1", "HTTP/1.0" or "HTTP/2.0": the protocol it came over (a server's) or went over (Go's r.Proto)
net::url url() const;
string header(const string& name) const;          // "" when there is none
http::headers& headers() const noexcept;
request& set_header(const string& name, const string& value);
request& add_header(const string& name, const string& value);
request& set_body(const string& text);
request& set_body(vector<byte> bytes);
request& set_body(const io::reader& stream, optional<uint64_t> length = nullopt);

// a received request
string path_value(const string& name) const;
string query(const string& name) const;
string cookie(const string& name) const;
optional<uint64_t> content_length() const noexcept;
net::endpoint remote_endpoint() const noexcept;
async::stop_token stop() const noexcept;
expected<string, io::error> text() const;
async::task<expected<string, io::error>> async_text() const;
expected<vector<byte>, io::error> bytes() const;
async::task<expected<vector<byte>, io::error>> async_bytes() const;
io::reader body() const;
http::headers trailers() const;
```

## Received by a handler

The server hands a request to the handler of the route it matched: the wildcards by `path_value`, the query by `query`, the fields by `header`:

```cpp
#include "sgcl/net/http/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /notes/{id}", [](net::http::request req, net::http::response_writer w) {
        w.write(req.method() + " note " + req.path_value("id") + " by " + req.query("author") + "\n");
    });
    srv.serve(":8080");
}
```

A request to it:

```text
$ curl http://localhost:8080/notes/42?author=Ann%20B
GET note 42 by Ann B
```

A handler that reads the body is a task and reads it with `co_await req.async_text()`, as on the [server](server.md) page.

## Example

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"

using namespace sgcl;

int main() {
    net::http::request note("POST", "https://httpbin.org/post");
    note.set_header("Content-Type", "text/plain");
    note.set_header("Cookie", "theme=dark; lang=pl");
    note.set_body("buy milk");
    println("{} {}", note.method(), note.header("Cookie"));

    net::http::client web;
    net::http::response res = web.send(note);
    string reply = res.text();
    println("{}, the note echoed: {}", res.status(), reply.contains("buy milk"));
}
```

Output:

```text
POST theme=dark; lang=pl
200, the note echoed: true
```

## See also

- [server](server.md) and [client](client.md), where a request is received and sent; [headers](headers.md), [cookie](cookie.md)
