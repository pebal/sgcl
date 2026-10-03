[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](../response_writer.md)

# sgcl::net::http::response_writer::write

```cpp
response_writer& write(const string& text) noexcept;               // (1)
response_writer& write(const slice<const byte>& data) noexcept;    // (2)
response_writer& write(const io::file& f);                         // (3)
template<class T>
response_writer& write(const T& text) noexcept;                    // (4)
```

Adds to the body, Go's `Write`. It never waits: the bytes go to a buffer in memory, sent with the head when the
handler returns (a body sent whole, with an exact Content-Length) or at the next [flush](flush.md) (chunked from the
first flush on).

1. The characters of `text`.
2. The bytes of `data`.
3. The file `f` from its position to its end; the position is moved to the end. When the file is all of the body
   (HTTP/1.1, nothing written before or after it, no flush), its length is the Content-Length and it goes after the
   head by `sendfile`: the file's pages go to the socket without a copy through the process, and over TLS it is read
   in blocks that are sealed where they lie. In any other case (bytes around it, a flush, HTTP/2) its bytes are taken
   into the body as (2) takes them. It writes the body only: Content-Type and the other fields stay the handler's. A
   file served whole is one line, `w.write(io::open(path))`.
4. A literal, a character array or a `std::string_view`, as (1). Takes part only for those, an exact match where the
   conversions to a string and to bytes would tie.

After the response has ended (a writer a task kept past its handler, once the server has sent the response), a write
is dropped.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the characters to add |
| `data` | the bytes to add; a `vector<byte>` or another slice converts to it |
| `f` | the file whose rest is the body or a part of it |

## Return value

`*this`.

## Complexity

- (1), (2), (4) Linear in the size of what is added, amortized.
- (3) Constant when the file is sent by `sendfile`, else linear in the rest of the file.

## Exceptions

None. A read of the file that fails (3) is the writer's first error, as a field that cannot be sent is: the flushes
return it, and the server answers 500 when nothing was sent yet.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    (void)io::write_file("page.html", "<p>a page</p>\n");
    net::http::server srv;
    srv.route("GET /page", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/html");
        w.write(io::open("page.html"));
    });
    srv.route("GET /parts", [](net::http::request, net::http::response_writer w) {
        w.write("text, ").write(vector<byte>{byte('b'), byte('y')}).write("tes\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    for (auto path : {"/page", "/parts"}) {
        net::http::response res = web.get(base + path);
        string text = res.text();
        print("{} {}", res.content_length().value_or(0), text);
    }
    srv.shutdown();
}
```

Output:

```text
14 <p>a page</p>
12 text, bytes
```

## See also

- [flush, async_flush](flush.md): sends what is buffered
- [sgcl::net::http::response_writer](../response_writer.md)
