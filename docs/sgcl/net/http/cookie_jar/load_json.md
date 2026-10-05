[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::load_json

```cpp
expected<void, io::error> load_json(const string& text) const noexcept;
```

The cookies of `text`, which [to_json](to_json.md) or [save](save.md) wrote, put in the jar beside the ones there:
each replaces a cookie of its name, domain, host-only flag and path, keeps the creation and last use it was written
with, and comes in the order of its creation; those whose `expires` has passed are left out, and `expires` is held to
400 days from now. The limits of the jar hold, the least recently used going first. A text is taken whole or not at
all: one that is not such a file, of another `version`, or with a cookie the jar could not have stored (a name that is
not a token, a control in the value, a domain that is not a host in lower case, a domain cookie of a public suffix, a
path that does not begin with `/`, a prefix's rule broken, a time that is not RFC 3339) changes nothing.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the JSON text of a jar |

## Return value

Nothing, or the error `net::errc::invalid_cookie` (operation `load cookie jar`).

## Complexity

Linear in the cookies of the text times the cookies of their sites, and their sort.

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
    jar.set_cookies(net::url("https://example.com/"), {net::http::cookie("login=abc; Max-Age=3600")});
    string kept = jar.to_json();

    net::http::cookie_jar later;
    println("{}", later.load_json(kept).has_value());
    println("{}", later.header(net::url("https://example.com/")));
    println("{}", later.load_json("{\"version\": 7}").error().message());
}
```

Output:

```text
true
login=abc
load cookie jar: invalid cookie
```

## See also

- [to_json](to_json.md): the text written
- [load](load.md): from a file
- [sgcl::net::http::cookie_jar](README.md)
