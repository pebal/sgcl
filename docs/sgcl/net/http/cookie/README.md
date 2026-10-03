[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::cookie

```cpp
#include "sgcl/net/http/cookie.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cookie;
}
```

`sgcl::net::http::cookie` is a cookie as a server sets it (`Set-Cookie`, RFC 6265 §4.1) and as a client reads one
back (§5.2): a value type with its attributes as public fields, Go's `http.Cookie`. [to_string](to_string.md)
writes the value of a `Set-Cookie` field, [parse](parse.md) reads one; a handler sends one with the
[response_writer](../response_writer/README.md)'s `add_cookie`, and a request's `Cookie` field is read by
[request::cookie](../request/cookie.md).

There is no jar: which cookies go to which request needs the public suffix list, which is not in the library yet.
`Expires` is a [time::datetime](../../../time/README.md), written as IMF-fixdate in GMT and read by the cookie-date
algorithm of RFC 6265 §5.1.1, which takes what browsers take.

## Rules

- A `cookie` is a value: a copy is a cookie of its own, and a moved-from one keeps its fields (a string's move
  copies its word). It holds strings, so it lives where a `tracked_ptr` may: on a
  stack, in a task, in a managed object; in a global or a `std` container, a [rooted](../../../core/rooted/README.md) of it.
- **Written**, a name that is not a token is `invalid_argument`; a byte of the value no cookie may hold is dropped and
  a value with a space or a comma is quoted, as Go does; a `Path` loses its `;` and controls; a `Domain` that is not
  a host name is left out; a `Max-Age` of zero or less is written `Max-Age=0` (the cookie is deleted now)
  ([to_string](to_string.md)).
- **Read** as a browser reads it: the attributes it does not understand, and those that are malformed, are ignored,
  and the error (`net::errc::invalid_cookie`) comes back only for a first pair without a `=` or with a name that is
  not a token ([parse](parse.md)).
- Where both `Expires` and `Max-Age` are given, `Max-Age` wins (a browser's rule; the cookie keeps both fields).

## Member objects

| Member | Description |
|---|---|
| `string name` | the name, a token of RFC 6265 |
| `string value` | the value |
| `string path` | `Path`: the cookie goes to this path and below; `""`, the default, for the default path |
| `string domain` | `Domain`: the host and its subdomains (`example.com`); `""`, the default, for the host alone |
| `optional<time::datetime> expires` | `Expires`; `nullopt` by default |
| `optional<duration> max_age` | `Max-Age`, in whole seconds: zero or less deletes the cookie now; `nullopt` by default |
| `bool secure` | `Secure`: sent over https only; `false` by default |
| `bool http_only` | `HttpOnly`: not given to scripts; `false` by default |
| `bool partitioned` | `Partitioned` (CHIPS): kept apart for each top-level site; `false` by default |
| `string same_site` | `SameSite`: `"Strict"`, `"Lax"`, `"None"`, or `""`, the default, for none |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cookie.md) | constructs a cookie: empty, of a name and a value, or of a `Set-Cookie` literal |
| `(destructor)` | drops the strings |
| `operator=` | copies or moves another cookie |
| [to_string](to_string.md) | the value of a `Set-Cookie` field |
| [parse](parse.md) | reads the value of a `Set-Cookie` field (static) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    net::http::cookie session("session", "abc 123");
    session.path = "/";
    session.max_age = std::chrono::hours(1);
    session.http_only = true;
    session.same_site = "Lax";
    println("{}", session.to_string());

    net::http::cookie back("id=42; Domain=.Example.COM; Secure; Max-Age=oops; "
                           "Expires=Wed, 09-Jun-2021 10:18:14 GMT");
    println("{}={} {} {} {}", back.name, back.value, back.domain, back.secure,
            back.max_age.has_value());
    println("{}", back.expires->format(time::rfc3339));
}
```

Output:

```text
session="abc 123"; Path=/; Max-Age=3600; HttpOnly; SameSite=Lax
id=42 example.com true false
2021-06-09T10:18:14Z
```

## See also

- [response_writer](../response_writer/README.md): `add_cookie`, a cookie sent by a handler
- [request::cookie](../request/cookie.md): a cookie of a request's `Cookie` field
- [headers](../headers/README.md): the `Set-Cookie` fields of a response, by `get_all`
