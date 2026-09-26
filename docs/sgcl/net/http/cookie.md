# sgcl::net::http::cookie

```cpp
#include "sgcl/net/http/cookie.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    class cookie;   // a cookie and its attributes as fields (RFC 6265)
}
```

A cookie as a server sets it (`Set-Cookie`, RFC 6265 §4.1) and as a client reads one back (§5.2): a value type with its attributes as public fields, Go's `http.Cookie`. There is no jar: which cookies go to which request needs the public suffix list, which is not in the library yet. A request's `Cookie` field is read by [`request::cookie(name)`](request.md).

## Rules

- **`to_string`** writes a `Set-Cookie` value: a name that is not a token is `invalid_argument`; a byte of the value no cookie may hold is dropped and a value with a space or a comma is quoted, as Go does; a `Path` loses its `;` and controls; a `Domain` that is not a host name is left out; `Max-Age` of zero or less is written `Max-Age=0` (the cookie is deleted now).
- **`parse`** reads one as a browser does: the attributes it does not understand, and those that are malformed (a `Max-Age` that is not a number, a `Path` that does not begin with `/`, a `SameSite` of another value), are ignored, and the error (`net::errc::invalid_cookie`) comes back only for a first pair without a `=` or with a name that is not a token. A leading `.` of a `Domain` is dropped and the domain lowercased; quotes around a value are taken off.
- **`Expires`** is a [`time::datetime`](../../time/README.md): written as IMF-fixdate in GMT, and read by the cookie-date algorithm of RFC 6265 §5.1.1, which takes what browsers take (`Wed, 09 Jun 2021 10:18:14 GMT`, Netscape's `Wed, 09-Jun-2021 10:18:14 GMT`, RFC 850, asctime): the first time, day, month and year among the tokens, a year of two digits 1970 to 2069, nothing before 1601; a date past 2262 (a "never" of 9999) is the end of `datetime`'s range. Where both are given, `Max-Age` wins (a browser's rule, the cookie keeps both fields).

## Members

```cpp
string name;
string value;
string path;
string domain;
optional<time::datetime> expires;
optional<duration> max_age;
bool secure = false;
bool http_only = false;
bool partitioned = false;
string same_site;                    // "Strict", "Lax", "None", or ""

cookie();
cookie(const string& name, const string& value);
string to_string() const;
static expected<cookie, io::error> parse(const string& set_cookie);
```

## Example

```cpp
#include "sgcl/net/http/cookie.h"
#include <iostream>

using namespace sgcl;

int main() {
    net::http::cookie c("session", "abc 123");
    c.path = "/";
    c.max_age = std::chrono::hours(1);
    c.http_only = true;
    c.same_site = "Lax";
    std::cout << c.to_string() << '\n';

    auto back = net::http::cookie::parse("id=42; Domain=.Example.COM; Secure; Max-Age=oops; Expires=Wed, 09-Jun-2021 10:18:14 GMT");
    std::cout << back->name << '=' << back->value << ' ' << back->domain << ' ' << back->secure << ' ' << bool(back->max_age) << '\n';
    std::cout << back->expires->format(time::rfc3339) << '\n';
}
```

Output:

```text
session="abc 123"; Path=/; Max-Age=3600; HttpOnly; SameSite=Lax
id=42 example.com 1 0
2021-06-09T10:18:14Z
```

## See also

- [response_writer](response_writer.md) (`set_cookie`), [request](request.md) (`cookie`)
