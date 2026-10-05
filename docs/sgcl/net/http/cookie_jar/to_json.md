[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::to_json

```cpp
string to_json(bool session_cookies = false) const noexcept;
```

The jar as JSON text, for a program that keeps it somewhere of its own (a database, a keychain) rather than in a
file ([save](save.md)): the persistent cookies that have not expired, and the session ones too when
`session_cookies` is `true`. [load_json](load_json.md) reads it back, with each cookie's creation and last use, so
the order of a `Cookie` field and the eviction go on as they were.

The file is a JSON object, `version` 1 and `cookies`, an array of objects in the order of the cookies' domains and
creation: `name`, `value`, `domain` (no dot in front), `host_only`, `path`, `expires` (RFC 3339 in UTC; left out for a
session cookie), `secure`, `http_only`, `same_site` and `partitioned` (left out when not set), `created` and
`last_access` (RFC 3339 with nanoseconds). A value or a path that is not UTF-8, which a cookie may be and JSON text
may not, is written in hexadecimal as `value_hex` or `path_hex` in its place.

## Parameters

| Parameter | Description |
|---|---|
| `session_cookies` | whether the session cookies are written too; `false` by default, as a browser keeps none across a restart |

## Return value

The text, indented by two spaces.

## Complexity

Linear in the cookies of the jar, and their sort.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::cookie_jar jar;
    jar.set_cookies(net::url("https://example.com/"),
                    {net::http::cookie("login=abc; Expires=Fri, 01 Jan 2100 00:00:00 GMT; Secure"),
                     net::http::cookie("visit=1")});
    string text = jar.to_json();
    println("{}", text.contains("login") && !text.contains("visit"));
    println("{}", jar.to_json(true).contains("visit"));
}
```

Output:

```text
true
true
```

## See also

- [load_json](load_json.md): the text read back
- [save](save.md): the text in a file
- [sgcl::net::http::cookie_jar](README.md)
