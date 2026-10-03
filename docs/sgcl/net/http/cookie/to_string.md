[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie](../cookie.md)

# sgcl::net::http::cookie::to_string

```cpp
string to_string() const;
```

Writes the cookie as the value of a `Set-Cookie` field (RFC 6265 §4.1), Go's `Cookie.String`: `name=value`, then the
attributes that are set, in the order `Path`, `Domain`, `Expires`, `Max-Age`, `HttpOnly`, `Secure`, `SameSite`,
`Partitioned`. What a field cannot hold is made safe as Go makes it:

- a byte of the value no cookie may hold (a control, `"`, `;`, `\`, a byte past ASCII) is dropped, and a value with a
  space or a comma is quoted;
- a `Path` loses its `;` and its controls;
- a `Domain` that is not a host name (letters, digits, `-`, `.` and `_`) is left out; one leading `.` is dropped;
- `Expires` is written as IMF-fixdate, always GMT (`Wed, 09 Jun 2021 10:18:14 GMT`);
- `Max-Age` is written in whole seconds, a part of a second dropped (999 ms is `Max-Age=0`), and one of zero or less
  as `Max-Age=0`, which deletes the cookie now;
- `SameSite` is written for `Strict`, `Lax` and `None`, in any case as set, and left out for any other value.

## Parameters

None.

## Return value

The value of the `Set-Cookie` field.

## Complexity

Linear in the sizes of the fields.

## Exceptions

`invalid_argument` when the name is not a token of RFC 6265: a broken contract of the program, not input to report.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    net::http::cookie c("note", "buy milk, bread;\"now\"");
    c.path = "/a;b";
    c.domain = ".example.com";
    c.expires = time::datetime::from_unix(1623233894);
    c.max_age = std::chrono::seconds(-1);
    c.same_site = "lax";
    c.partitioned = true;
    println("{}", c.to_string());

    net::http::cookie broken("my name", "x");
    try {
        println("{}", broken.to_string());
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
note="buy milk, breadnow"; Path=/ab; Domain=example.com; Expires=Wed, 09 Jun 2021 10:18:14 GMT; Max-Age=0; SameSite=Lax; Partitioned
http::cookie: a name must be a token of RFC 6265
```

## See also

- [parse](parse.md): the other direction
- [sgcl::net::http::cookie](../cookie.md)
