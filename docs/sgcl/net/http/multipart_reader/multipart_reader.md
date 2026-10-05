[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [multipart_reader](README.md)

# sgcl::net::http::multipart_reader::multipart_reader

```cpp
multipart_reader() noexcept;                                                                   // (1)
multipart_reader(const io::reader& body, const string& boundary) noexcept;                     // (2)
multipart_reader(const io::reader& body, const string& boundary, const limits& l) noexcept;    // (3)
multipart_reader(const multipart_reader&) = default;                                           // (4), implicitly declared
```

1. No body: an operation on it is a contract violation; `operator bool` is `false`.
2. A reader of `body`, its parts apart by `boundary` (the `boundary` parameter of its `Content-Type`), within the
   default [limits](../multipart_reader-limits.md): 1000 parts, 16 KB a head. Go's `multipart.NewReader`.
3. The same within `l`.
4. The same reader: a copy shares its state.

- (2–3) Nothing is read now. A boundary of no character or of more than 70 (RFC 2046 §5.1.1) makes a reader whose
  first [next](next.md) is `net::errc::malformed_multipart`.

## Parameters

| Parameter | Description |
|---|---|
| `body` | the stream of the body |
| `boundary` | the boundary, without the two dashes before it |
| `l` | the parts a body may have, the bytes of a head (at most 32 KB) |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string body = "--b\r\n\r\none\r\n--b\r\n\r\ntwo\r\n--b\r\n\r\nthree\r\n--b--\r\n";
    net::http::multipart_reader::limits two;
    two.max_parts = 2;
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), "b", two);
    while (auto p = parts.next()) {
        if (!*p) {
            break;
        }
        println("{}", parts.read_all_text().value());
    }
    println("{}", parts.next().error().message());

    net::http::multipart_reader none;
    println("{}", (bool)none);
}
```

Output:

```text
one
two
multipart: too many parts
false
```

## See also

- [request::multipart](../request/multipart.md): a request's body and boundary
- [multipart_reader](README.md)
