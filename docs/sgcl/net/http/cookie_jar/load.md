[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cookie_jar](README.md)

# sgcl::net::http::cookie_jar::load, async_load

```cpp
expected<void, io::error> load(const string& path) const;                         // (1)
async::task<expected<void, io::error>> async_load(string path) const noexcept;    // (2)
```

The cookies of the file at `path`, which [save](save.md) wrote, put in the jar as [load_json](load_json.md) puts
them: beside the ones there, each replacing a cookie of its identity, the expired left out, the limits held, the file
taken whole or not at all.

1. On the calling thread.
2. The same in a task, the file read on the blocking pool.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

Nothing, or an [io::error](../../../io/error/README.md): the read's (`ENOENT` for a jar never saved, which a program
takes as an empty jar), or `net::errc::invalid_cookie` with the path for a file that is not a jar's.

## Complexity

Linear in the cookies of the file times the cookies of their sites, and the read.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::cookie_jar first;
    first.set_cookies(net::url("https://example.com/"), {net::http::cookie("login=abc; Max-Age=86400")});
    first.save("cookies.json");

    net::http::cookie_jar next;
    println("{}", next.load("cookies.json").has_value());
    println("{}", next.header(net::url("https://example.com/")));
    println("{}", next.load("missing.json").error().is_not_found());
}
```

Output:

```text
true
login=abc
true
```

## See also

- [save](save.md): the file written
- [load_json](load_json.md): from text
- [sgcl::net::http::cookie_jar](README.md)
