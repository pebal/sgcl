[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::with_path

```cpp
expected<url, io::error> with_path(const string& path) const noexcept;
```

The URL with the path given, by the standard's pathname setter: the path is read as the parser reads one, escaped and
resolved, and in a special URL `\` is `/`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the new path |

## Return value

The new URL, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url` ([errc](../errc.md)), the
operation `set URL path` and the value asked for, when the standard refuses the value or declines to apply it: a URL
with an opaque path; and a value past 512 MiB, or a URL that would pass it ([the limit](../url.md#rules)).

## Complexity

Linear in the length of the URL and of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("https://example.com/a?q=1");
    println(u.with_path("/docs/a b/../c")->to_string());
    println(u.with_path("index.html")->to_string());
    println(net::url("mailto:x@example.com").with_path("/y").error().message());
}
```

Output:

```text
https://example.com/docs/c?q=1
https://example.com/index.html?q=1
set URL path /y: invalid URL
```

## See also

- [path](path.md): the path
- [sgcl::net::url](../url.md)
