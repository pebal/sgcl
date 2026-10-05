[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [multipart_reader](multipart_reader/README.md) › limits

# sgcl::net::http::multipart_reader::limits

```cpp
#include "sgcl/net/http/multipart.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class multipart_reader {
    public:
        struct limits {
            size_t max_parts = 1000;
            size_t max_header_bytes = 16384;
        };
    };
}
```

`sgcl::net::http::multipart_reader::limits` is how much a multipart body may hold before the reader refuses it: the
number of its parts and the bytes of one part's head, as Go bounds its reader (`multipartmaxparts`). The whole body is
not bounded here but by its stream: a request's by the server's `max_body_bytes`. A plain struct, given to the
[constructor](multipart_reader/multipart_reader.md) or to [request::multipart](request/multipart.md).

## Member objects

| Member | Description |
|---|---|
| `max_parts` | the most parts; the one past is `net::errc::too_many_parts`; 1000 by default |
| `max_header_bytes` | the most bytes of one part's head, its empty line included; past it `net::errc::header_too_large`; 16 KB by default, at most the window's 32 KB less one |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string body = "--b\r\nX-Long: " + string(100, 'h') + "\r\n\r\nv\r\n--b--\r\n";
    net::http::multipart_reader::limits small;
    small.max_header_bytes = 64;
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), "b", small);
    println("{}", parts.next().error().message());
}
```

Output:

```text
multipart: header too large
```

## See also

- [(constructor)](multipart_reader/multipart_reader.md), [request::multipart](request/multipart.md): what take them
- [multipart_reader](multipart_reader/README.md)
