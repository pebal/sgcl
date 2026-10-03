[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::path

```cpp
string path() const noexcept;
```

The path, escaped as the URL writes it, Go's `URL.EscapedPath()`: `/a%20b` of `http://x/a b`. The path of a special
URL is resolved and never empty (`/` at least); the opaque path of a URL without a hierarchical one is the whole of
what follows the `:` (`x@example.com` of `mailto:x@example.com`).

## Parameters

None.

## Return value

The path; the empty string for a URL of another scheme without one.

## Complexity

Linear in the length of the part.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    for (const char* text : {"http://x/a b/../c", "http://x", "mailto:x@example.com", "sc://h"}) {
        println("{} -> [{}]", text, net::url(text).path());
    }
}
```

Output:

```text
http://x/a b/../c -> [/c]
http://x -> [/]
mailto:x@example.com -> [x@example.com]
sc://h -> []
```

## See also

- [with_path](with_path.md): another path
- [request_target](request_target.md): the path and the query
- [sgcl::net::url](README.md)
