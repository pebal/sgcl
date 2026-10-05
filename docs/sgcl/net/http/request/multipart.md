[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::multipart

```cpp
expected<multipart_reader, io::error> multipart() const noexcept;                                     // (1)
expected<multipart_reader, io::error> multipart(const multipart_reader::limits& l) const noexcept;    // (2)
```

The parts of a received request's body, read as they come: a [multipart_reader](../multipart_reader/README.md) over
[body](body.md) with the boundary of its `Content-Type`, Go's `r.MultipartReader()`. Nothing is read here; the
reader reads the body as its parts are asked for, bounded by the server's `max_body_bytes`, and holds no more of it
than its window of 32 KB, so an upload of any size goes through it to a file.

1. Within the default [limits](../multipart_reader-limits.md): 1000 parts, 16 KB a part's head.
2. Within `l`.

Any `multipart/` type is taken (`form-data`, `mixed`, `related`); the boundary is the parameter of that name, a
token or a quoted-string of 1 to 70 characters.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the parts the body may have, the bytes of a head |

## Return value

The reader; or the [io::error](../../../io/error/README.md), operation `multipart` and path the `Content-Type`:
`net::errc::not_multipart` for a request whose `Content-Type` is not `multipart/...` (or that has none),
`net::errc::malformed_multipart` for one without a boundary or with one of more than 70 characters.

## Complexity

Linear in the length of the `Content-Type`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("POST /upload", [](net::http::request req, net::http::response_writer w)
                                  -> async::task<> {
        auto parts = req.multipart();
        if (!parts) {
            w.error(net::http::status::bad_request);
            co_return;
        }
        while (auto p = co_await parts->async_next()) {
            if (!*p) {
                break;
            }
            if (!(*p)->filename.empty()) {
                io::file out = (co_await io::async_create("saved-" + (*p)->filename)).value();
                uint64_t n = (co_await io::async_copy(out, *parts)).value();
                (void)out.close();
                w.write("saved " + (*p)->filename + ", " + to_string(n) + " bytes\n");
            }
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/upload";

    io::write_file("photo.jpg", string(250000, 'p'));
    net::http::client web;
    print("{}", web.post(url, net::http::form{net::http::form::file("photo", "photo.jpg")})->text().value());
    println("{}", web.post(url, "text/plain", "not a form")->status());
    srv.close();
}
```

Output:

```text
saved photo.jpg, 250000 bytes
400
```

## See also

- [form, async_form](form.md): the fields alone, as pairs
- [multipart_reader](../multipart_reader/README.md): the reader
- [sgcl::net::http::request](README.md)
