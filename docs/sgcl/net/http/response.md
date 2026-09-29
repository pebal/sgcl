# sgcl::net::http::response

```cpp
#include "sgcl/net/http/response.h"   // or "sgcl/net/http/http.h", "sgcl/sgcl.h"

namespace sgcl::net::http {
    class response;   // what a client receives; a handle of one word
}
```

A response as [`client`](client.md) returns it: the status and the head read, the body still on the connection. A 4xx or a 5xx is a response and not an error, as in Go: `ok()` tells a 2xx.

## Rules

- **The body** is read once, with `text()`, `bytes()`, `json()`, `save(path)` or the stream `body()`, and its end gives the connection back to the client's pool, with no close. In a task, `co_await res.async_text()`; `text()` blocks the thread.
- **`close()`** gives the body up without waiting: when the rest of it is already in the connection's buffer it is dropped and the connection goes back to the pool, otherwise the connection is closed. A response neither read nor closed keeps its connection out of the pool until the collector finds it. The stream `body()` has no close of its own (`has_close()` is `false`, and its `close()` does nothing): the body is given up by the response's `close()`, not by Go's `resp.Body.Close()`.
- A body cut short is `io::errc::unexpected_eof`, a chunked framing broken `malformed_response`, a total `timeout` of the client passing while it is read `ETIMEDOUT`.
- `url()` is the URL the response came from, the last of the redirects; `trailers()` holds the trailer fields of a chunked body once it has been read.
- **`proto()`** is the protocol the response came over, as Go's `resp.Proto`: `"HTTP/2.0"`, `"HTTP/1.1"` or `"HTTP/1.0"`. Over HTTP/2 the body is the stream's DATA, its window given back to the server as it is read. `close()` of a body not read to its end resets the stream (`CANCEL`) and leaves the connection to the other requests. The trailers are the fields the server sent after the body, and a stream the server resets while its body is read is `std::errc::connection_reset`.

## Members

```cpp
int status() const noexcept;
string proto() const;                    // "HTTP/1.1", "HTTP/1.0" or "HTTP/2.0": the protocol it came over (Go's resp.Proto)
bool ok() const noexcept;                          // 200 to 299
string header(const string& name) const;           // "" when there is none
const http::headers& headers() const noexcept;
optional<uint64_t> content_length() const noexcept;
net::url url() const;
expected<string, io::error> text() const;
async::task<expected<string, io::error>> async_text() const;
expected<vector<byte>, io::error> bytes() const;
async::task<expected<vector<byte>, io::error>> async_bytes() const;
expected<encoding::json, io::error> json() const;          // the body as JSON
template<class T> expected<T, io::error> json() const;     // through describe()
expected<uint64_t, io::error> save(const string& path) const;   // into the file, through path + ".part"
// + async_json(), async_json<T>(), async_save(path)
io::reader body() const;
http::headers trailers() const;
void close() const;
```

## Examples

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"

using namespace sgcl;

int main() {
    net::http::client web;
    net::http::response res = web.get("https://www.apache.org/licenses/LICENSE-2.0.txt");
    println("{} {}, {} bytes declared", res.status(), res.ok(), res.content_length().value_or(0));
    string text = res.text();
    println("{} bytes read", text.size());
}
```

Output:

```text
200 true, 11357 bytes declared
11357 bytes read
```

### JSON

`json()` gives the body as an [`encoding::json`](../../encoding/json.md) value, `json<T>()` as a struct of the program's, through its `describe`:

```cpp
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"

using namespace sgcl;

struct echo {
    string data;

    void describe(encoding::field_list& f) {
        f.add("data", data);
    }
};

int main() {
    net::http::client web;
    net::http::response res = web.post("https://httpbin.org/post", "text/plain", "buy milk");
    echo reply = res.json<echo>();
    println("{} {}", res.status(), reply.data);
}
```

Output:

```text
200 buy milk
```

### Into a file

`save(path)` writes the body through `path + ".part"`, renamed at its end, and gives the number of bytes; unlike `download` it saves any status:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"

using namespace sgcl;

int main() {
    net::http::client web;
    net::http::response res = web.get("https://www.apache.org/licenses/LICENSE-2.0.txt");
    uint64_t saved = res.save("LICENSE-2.0.txt");
    println("{}, {} bytes saved", res.status(), saved);
}
```

Output:

```text
200, 11357 bytes saved
```

### Statuses and redirects

A 4xx or a 5xx is a response, not an error; `url()` is where the redirects ended; `close()` gives a body up unread:

```cpp
#include "sgcl/async/async.h"
#include "sgcl/io/io.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /old", [](net::http::request, net::http::response_writer w) { w.redirect("/new"); });
    srv.route("GET /new", [](net::http::request, net::http::response_writer w) { w.write("moved here\n"); });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    auto base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response moved = web.get(base + "/old");
    println("{} {}", moved.status(), moved.url().path());
    moved.close();
    net::http::response missing = web.get(base + "/nothing");
    println("{} {}", missing.status(), missing.ok());
    missing.close();
    srv.close();
}
```

Output:

```text
200 /new
404 false
```

## See also

- [client](client.md): where it comes from; [headers](headers.md), [cookie](cookie.md) (`cookie::parse` of a `Set-Cookie`)
