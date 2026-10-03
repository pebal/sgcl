[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::query

```cpp
string query() const noexcept;
```

The query without its `?`, escaped as the URL writes it: Go's `URL.RawQuery`. Its pairs, unescaped, are
[query_params](query_params.md).

## Parameters

None.

## Return value

The query; the empty string when there is none or it is empty ([has_query](has_query.md) tells the two apart).

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
    net::url u("https://x/search?q=a b&lang=pl");
    println("{} [{}]", u.query(), net::url("https://x/").query());
}
```

Output:

```text
q=a%20b&lang=pl []
```

## See also

- [query_params](query_params.md): the pairs
- [with_query](with_query.md): another query
- [sgcl::net::url](README.md)
