[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::multipart_reader

```cpp
#include "sgcl/net/http/multipart.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class multipart_reader {
    public:
        struct part;     // what a part's head says
        struct limits;   // parts, the bytes of a head
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::multipart_reader` reads a multipart body as it comes (RFC 2046 §5.1; `multipart/form-data` of RFC
7578): [next](next.md) goes to the next part, past whatever of the current one was not read, and gives its head; the
reads ([read](read.md), and every read of io's reader mixin: `read_all`, `copy_to`, `io::copy` from it) give that
part's content, to its end. Go's `multipart.Reader` and its `Part` in one, as
[compress::tar::reader](../../../compress/tar-reader/README.md) is a tar archive's entries and their data. Nothing of the
body is held but a window of 32 KB: a file of gigabytes goes through it to wherever the program copies it.

A handler gets one from its request, [request::multipart](../request/multipart.md), the boundary taken from the
`Content-Type`; any multipart body over a stream is read by the constructor with its boundary. The fields alone, as
name and value pairs, are [request::form](../request/form.md).

## Rules

- The preamble before the first boundary and the epilogue after the close delimiter are dropped. A boundary counts at
  a line's start only, followed by `--` (the close delimiter) or by transport padding (spaces and tabs) and a line
  break; the boundary's text anywhere else in a part, or followed by anything else, is content.
- Lines end in CRLF, or in LF alone when the first boundary's line ends so, as Go reads them.
- A part's head is read by the HTTP field parser of the module (no obs-fold, no control bytes), within
  `limits::max_header_bytes` (16 KB; `net::errc::header_too_large` past it); a body of more than `limits::max_parts`
  parts (1000, Go's default) is `net::errc::too_many_parts` at the one past. The body's whole size is its stream's to
  bound: a request's is the server's `max_body_bytes`.
- A body that ends before its close delimiter is `io::errc::unexpected_eof`; one with no boundary at all
  `net::errc::malformed_multipart`. An error ends the reader: every later call gives it.
- [part](../multipart_reader-part.md)'s `filename` is the last element of what was sent (`../../etc/passwd` is
  `passwd`), as Go's `FileName`; `filename*` (RFC 8187, UTF-8) is taken before `filename`.
- `next` and the reads have a blocking form and one for a task; over a request's body the blocking ones are for a
  thread of the program, as [request::text](../request/text.md) is: a handler awaits `async_next` and `async_read`.
- A handle of one word: a copy is the same reader.

## Member types

| Type | Definition |
|---|---|
| [part](../multipart_reader-part.md) | what a part's head says: name, filename, content type, fields |
| [limits](../multipart_reader-limits.md) | the parts a body may have, the bytes of a head |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](multipart_reader.md) | a reader of a body with its boundary, or none |
| [next, async_next](next.md) | goes to the next part and gives its head |
| [read, async_read](read.md) | reads the current part's content |
| [operator bool](operator_bool.md) | whether there is a body |

#### From mixin::reader

| Function | Description |
|---|---|
| [read_full, async_read_full](../../../io/mixin/reader/read_full.md) | fills the whole buffer from the part's content |
| [read_all, async_read_all](../../../io/mixin/reader/read_all.md) | the part's content, as bytes |
| [read_all_text, async_read_all_text](../../../io/mixin/reader/read_all_text.md) | the part's content, as text |
| [copy_to, async_copy_to](../../../io/mixin/reader/copy_to.md) | the part's content, written to a writer |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string body = "preamble\r\n"
                  "--XyZ\r\n"
                  "Content-Disposition: form-data; name=\"title\"\r\n"
                  "\r\n"
                  "holidays\r\n"
                  "--XyZ\r\n"
                  "Content-Disposition: form-data; name=\"photo\"; filename=\"beach.jpg\"\r\n"
                  "Content-Type: image/jpeg\r\n"
                  "\r\n"
                  "JPEG bytes\r\n"
                  "--XyZ--\r\n";
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), "XyZ");
    while (auto p = parts.next()) {
        if (!*p) {
            break;  // the end
        }
        println("{} [{}] [{}]: {}", (*p)->name, (*p)->filename, (*p)->content_type,
                parts.read_all_text().value());
    }
}
```

Output:

```text
title [] []: holidays
photo [beach.jpg] [image/jpeg]: JPEG bytes
```

## See also

- [request::multipart](../request/multipart.md), [request::form](../request/form.md): a request's body
- [form](../form/README.md): such a body to send
- RFC 2046 §5.1, RFC 7578; `tests/net/http/multipart.cpp` (every edge, a body in pieces of every size, Go's
  `mime/multipart` writing what this reads), `tests/net/http/fuzz/http_multipart_fuzz.cpp`
