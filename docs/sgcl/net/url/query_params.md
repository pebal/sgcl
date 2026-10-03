[sgcl](../../README.md) › [net](../README.md) › [url](README.md)

# sgcl::net::url::query_params

```cpp
net::query_params query_params() const noexcept;
```

The pairs of the query, unescaped, as [query_params::parse](../query_params/parse.md) reads them
(`application/x-www-form-urlencoded`): Go's `URL.Query()`. A name may come many times, in its order.

It never fails. The query is within [the limit](README.md#rules) of 512 MiB, and its pairs are taken whole, though
written by the form's rules they may take up to three times it (a `/` the query keeps and the form escapes), where
`parse` would refuse them: [add](../query_params/add.md) and [set](../query_params/set.md) refuse to grow such pairs,
and [with_query](with_query.md) refuses them ([the limit](../query_params/README.md#rules) of query_params).

## Parameters

None.

## Return value

The pairs; none when there is no query.

## Complexity

Linear in the length of the URL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("https://x/search?q=a+b&tag=c%2B%2B&tag=go");
    auto params = u.query_params();
    println("{} {}", params.get("q"), params.get_all("tag").size());
    for (auto& [name, value] : params) {
        println("{}={}", name, value);
    }
}
```

Output:

```text
a b 2
q=a b
tag=c++
tag=go
```

## See also

- [query](query.md): the query as it is written
- [with_query](with_query.md): a URL with pairs as its query
- [sgcl::net::url](README.md)
