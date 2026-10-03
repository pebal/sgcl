[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie](../cookie.md)

# sgcl::net::http::cookie::parse

```cpp
static expected<cookie, io::error> parse(const string& field) noexcept;
```

Reads the value of a `Set-Cookie` field as a browser reads it (RFC 6265 §5.2), Go's `http.ParseSetCookie`: the first
pair is the name and the value, quotes around the value taken off, and the attributes follow, their names in any
case. An attribute it does not understand is ignored, and so is one that is malformed: a `Max-Age` that is not a
number (an optional `-` and digits), a `Path` that does not begin with `/`, a `SameSite` of another value than
`Strict`, `Lax` or `None` (written back in that case), an `Expires` that is no date. A leading `.` of a `Domain` is
dropped and the domain lowercased. A `Max-Age` of zero or less is read as zero; one of any length is a number,
leading zeros among it, and one past the range of `duration` (292 years) is read as `duration::max()`.

`Expires` is read by the cookie-date algorithm of RFC 6265 §5.1.1, which takes what browsers take
(`Wed, 09 Jun 2021 10:18:14 GMT`, Netscape's `Wed, 09-Jun-2021 10:18:14 GMT`, RFC 850's
`Wednesday, 09-Jun-21 10:18:14 GMT`, asctime's `Wed Jun  9 10:18:14 2021`): the text cut at its delimiters, and of
its tokens the first that is a time, a day of the month, a month and a year, each once. A year of two digits is 1970
to 2069, nothing before 1601 is a date, and the instant is in UTC; a date past 2262 (a "never" of 9999) is the end of
the range of `time::datetime`.

## Parameters

| Parameter | Description |
|---|---|
| `field` | the value of a `Set-Cookie` field |

## Return value

The cookie, or the [error](../../../io/error.md) `net::errc::invalid_cookie` ([errc](../../errc.md)) for a first pair
without a `=` or with a name that is not a token.

## Complexity

Linear in the size of `field`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    for (const char* field : {"id=\"42\"; path=/a; HTTPONLY; Colour=blue; SameSite=sometimes",
                              "old=1; Expires=Wednesday, 09-Jun-21 10:18:14 GMT; Max-Age=-5",
                              "never=1; Expires=Fri, 31 Dec 9999 23:59:59 GMT",
                              "no equals sign"}) {
        auto c = net::http::cookie::parse(field);
        if (!c) {
            println("{}", c.error().message());
            continue;
        }
        println("{}={} path={} http_only={} same_site=[{}] max_age={} expires={}", c->name,
                c->value, c->path, c->http_only, c->same_site, c->max_age.has_value(),
                c->expires ? c->expires->format(time::rfc3339) : string("none"));
    }
}
```

Output:

```text
id=42 path=/a http_only=true same_site=[] max_age=false expires=none
old=1 path= http_only=false same_site=[] max_age=true expires=2021-06-09T10:18:14Z
never=1 path= http_only=false same_site=[] max_age=false expires=2262-04-11T23:47:16Z
parse cookie no equals sign: invalid cookie
```

## See also

- [to_string](to_string.md): the other direction
- [(constructor)](cookie.md): a `Set-Cookie` literal of the program
- [sgcl::net::http::cookie](../cookie.md)
