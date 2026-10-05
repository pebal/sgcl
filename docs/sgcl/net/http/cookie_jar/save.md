[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::save, async_save

```cpp
expected<void, io::error> save(const string& path, bool session_cookies = false) const;            // (1)
async::task<expected<void, io::error>> async_save(string path,                                     // (2)
                                                  bool session_cookies = false) const noexcept;
```

The jar written to the file at `path` as [to_json](to_json.md) writes it, so that a program keeps its logins across
runs and [load](load.md) gives them back. The file is written as `path` + `.tmp`, readable and writable by its owner
alone (mode 0600: cookies are credentials), and renamed over `path`, so a crash leaves the old file or the new one,
never half of one.

1. On the calling thread.
2. The same in a task, the file written on the blocking pool.

The file is a JSON object, `version` 1 and `cookies`, an array of objects in the order of the cookies' domains and
creation: `name`, `value`, `domain` (no dot in front), `host_only`, `path`, `expires` (RFC 3339 in UTC; left out for a
session cookie), `secure`, `http_only`, `same_site` and `partitioned` (left out when not set), `created` and
`last_access` (RFC 3339 with nanoseconds). A value or a path that is not UTF-8, which a cookie may be and JSON text
may not, is written in hexadecimal as `value_hex` or `path_hex` in its place.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `session_cookies` | whether the session cookies are written too; `false` by default |

## Return value

Nothing, or the [io::error](../../../io/error/README.md) of the write or the rename, with the path.

## Complexity

Linear in the cookies of the jar, and the write.

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
    jar.set_cookies(net::url("https://example.com/"), {net::http::cookie("login=abc; Max-Age=86400")});
    println("{}", jar.save("cookies.json").has_value());
    println("{}", io::stat("cookies.json")->mode == io::permissions(0600));
}
```

Output:

```text
true
true
```

## See also

- [load](load.md): the file read back
- [to_json](to_json.md): the text alone
- [sgcl::net::http::cookie_jar](README.md)
