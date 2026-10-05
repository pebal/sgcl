[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::form

```cpp
#include "sgcl/net/http/form.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class form {
    public:
        class part;   // a field, or a file (form::file)
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::form` is a `multipart/form-data` body to send (RFC 7578, RFC 2046 §5.1): fields and files in their
order, what an HTML form with a file input sends. `web.post(url, net::http::form{{"name", "value"},
net::http::form::file("upload", "a.png")})` is the whole of an upload. Go writes one with `mime/multipart.Writer`
into a buffer or a pipe; here the form is a description, and the client reads each file from disk as the body goes
out, never whole in memory, with the body's length known before it starts.

A form is a handle of one word, as a [request](../request/README.md) is: a copy is the same form. Its boundary is drawn
at random when it is made, 32 hexadecimal characters, which no content is to hold; a file is opened when its part is
reached and its size taken when the body is made, the `Content-Length` the sum of the parts.

## Rules

- A field is a name and a text (`{"name", "value"}`), sent without a `Content-Type` of its own (`text/plain` by RFC
  7578). A file ([file](file.md)) is a name, a path and a type: its name in the body is the path's last element, its
  type the one given or the one of its extension (`application/octet-stream` for one not known).
- Names and file names go in quoted-strings, a backslash before `"` and `\` (as Go and curl write them), a control
  byte but HTAB percent-encoded (`%0D`, `%0A`, as the HTML standard writes CR and LF), so that no name breaks the
  part's head; a type's control bytes go as spaces.
- [client::post](../client/post.md) and [request::set_body](../request/set_body.md) send it: `Content-Type` the form's
  ([content_type](content_type.md)), its length taken when the send begins, each file read as the body goes out. A
  form can be sent again (a retry on a new connection, a 307), its files read again. A file that cannot be read when
  the send begins fails it before anything is dialed; one that shrank since its size was taken fails it with
  `io::errc::unexpected_eof`, one that grew sends the bytes of the size it had.
- [reader](reader.md) is the body as a stream, for a program that writes it elsewhere (a file, another protocol).

## Member types

| Type | Definition |
|---|---|
| [part](../form-part.md) | one part of a form: a field, or a file |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](form.md) | an empty form, or one of the parts given |
| [file](file.md) | a file's part (static) |
| [add](add.md) | adds a field or a part after the others |
| [boundary](boundary.md) | the boundary between the parts |
| [content_type](content_type.md) | `multipart/form-data; boundary=...` |
| [content_length](content_length.md) | the body's length, the files' sizes taken now |
| [reader](reader.md) | the body as a stream |

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
        net::http::multipart_reader parts = req.multipart().value();
        while (auto p = co_await parts.async_next()) {
            if (!*p) {
                break;
            }
            vector<byte> content = (co_await parts.async_read_all()).value();
            w.write((*p)->name + " " + (*p)->filename + " " + to_string(content.size()) + "\n");
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/upload";

    io::write_file("notes.txt", "a file of 21 bytes.\n\n");
    net::http::client web;
    net::http::response res = web.post(url, net::http::form{{"title", "my notes"},
                                                            net::http::form::file("doc", "notes.txt")});
    print("{}", res.text().value());
    srv.close();
}
```

Output:

```text
title  8
doc notes.txt 21
```

## See also

- [client::post](../client/post.md), [request::set_body](../request/set_body.md): what sends it
- [multipart_reader](../multipart_reader/README.md): the other side, a multipart body read as it comes
- [query_params](../../query_params/README.md): `application/x-www-form-urlencoded`
- RFC 7578, RFC 2046 §5.1; `tests/net/http/multipart.cpp` (the body byte for byte, Go's `mime/multipart` reading it,
  curl `-F`)
