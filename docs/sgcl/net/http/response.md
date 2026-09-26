# sgcl::net::http::response

```cpp
#include "sgcl/net/http/response.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class response;   // what a client receives; a handle of one word
}
```

A response as [`client`](client.md) returns it: the status and the head read, the body still on the connection. A 4xx or a 5xx is a response and not an error, as in Go: `ok()` tells a 2xx.

## Rules

- **The body** is read once, with `text()`, `bytes()` or the stream `body()`, and its end gives the connection back to the client's pool, with no close. In a task, `co_await res->async_text()`; `text()` blocks the thread.
- **`close()`** gives the body up without waiting: when the rest of it is already in the connection's buffer it is dropped and the connection goes back to the pool, otherwise the connection is closed. A response neither read nor closed keeps its connection out of the pool until the collector finds it. The stream `body()` has no close of its own (`has_close()` is `false`, and its `close()` does nothing): the body is given up by the response's `close()`, not by Go's `resp.Body.Close()`.
- A body cut short is `io::errc::unexpected_eof`, a chunked framing broken `malformed_response`, a total `timeout` of the client passing while it is read `ETIMEDOUT`.
- `url()` is the URL the response came from, the last of the redirects; `trailers()` holds the trailer fields of a chunked body once it has been read.

## Members

```cpp
int status() const noexcept;
bool ok() const noexcept;                          // 200 to 299
string header(const string& name) const;           // "" when there is none
const http::headers& headers() const noexcept;
optional<uint64_t> content_length() const noexcept;
net::url url() const;
expected<string, io::error> text() const;
async::task<expected<string, io::error>> async_text() const;
expected<vector<byte>, io::error> bytes() const;
async::task<expected<vector<byte>, io::error>> async_bytes() const;
io::reader body() const;
http::headers trailers() const;
void close() const;
```

## See also

- [client](client.md): where it comes from; [headers](headers.md), [cookie](cookie.md) (`cookie::parse` of a `Set-Cookie`)
