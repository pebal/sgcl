[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::form, async_form

```cpp
expected<net::query_params, io::error> form() const;                                // (1)
async::task<expected<net::query_params, io::error>> async_form() const noexcept;    // (2)
```

The fields of a form a received request's body holds, as name and value pairs in their order
([query_params](../../query_params/README.md)), Go's `r.ParseForm` and `r.ParseMultipartForm` with `r.PostForm`:

- of `application/x-www-form-urlencoded`, the body read and its pairs parsed;
- of `multipart/form-data`, every part with a name and no file name, its content the value; a file's part is read past
  (its bytes are [multipart](multipart.md)'s to read);
- of another `Content-Type`, or none, no pair, and the body left unread.

The values together are at most 10 MB, as Go keeps them in memory (`net::errc::body_too_large` past it), within the
server's `max_body_bytes`. The URL's query is not among them: [query](query.md) reads it.

1. Blocks the calling thread: the reading runs on the scheduler and the thread waits for it. For a thread of the
   program, never a handler.
2. Returns a task that does the same: a handler writes `co_await req.async_form()`.

## Parameters

None.

## Return value

The pairs; or the [io::error](../../../io/error/README.md) of the body's reading, of a multipart body
([next](../multipart_reader/next.md)'s), `net::errc::body_too_large` past 10 MB of values.

## Complexity

Linear in the size of the body.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("POST /signup", [](net::http::request req, net::http::response_writer w)
                                  -> async::task<> {
        net::query_params fields = (co_await req.async_form()).value();
        w.write(fields.get("user") + " " + fields.get("plan") + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/signup";

    net::http::client web;
    print("{}", web.post(url, "application/x-www-form-urlencoded", "user=ann&plan=free")->text().value());
    print("{}", web.post(url, net::http::form{{"user", "bob"}, {"plan", "pro"}})->text().value());
    srv.close();
}
```

Output:

```text
ann free
bob pro
```

## See also

- [multipart](multipart.md): the parts as they come, files among them
- [query_params](../../query_params/README.md): the pairs
- [sgcl::net::http::request](README.md)
