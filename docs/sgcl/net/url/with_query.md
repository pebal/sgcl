[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::url::with_query

```cpp
expected<url, io::error> with_query(const string& query) const noexcept;                // (1)
expected<url, io::error> with_query(const net::query_params& params) const noexcept;    // (2)
```

The URL with the query given. The standard takes any query; the one refusal is [the limit](../url.md#rules) of
512 MiB the other setters keep.

1. By the standard's search setter: the query as written, escaped with the standard's query set; a leading `?` is
   dropped, and the empty string removes the query.
2. The pairs written as [query_params::to_string](../query_params/to_string.md) writes them, by the update steps of
   the standard's `URLSearchParams`; no pairs remove the query.

## Parameters

| Parameter | Description |
|---|---|
| `query` | the new query, with its `?` or without it; empty for none |
| `params` | the pairs of the new query |

## Return value

The new URL, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url` ([errc](../errc.md)) and the
operation `set URL query`:

- (1) with the value asked for, when it is past 512 MiB or the URL would pass it (a byte escaped is three);
- (2) with the pairs as written, when the URL would pass 512 MiB, and with no value for pairs written past 512 MiB,
  which only the pairs of a long query of a URL ([query_params](query_params.md)) are.

## Complexity

Linear in the length of the URL and of the query.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::url u("https://x/search?q=old#top");
    println(u.with_query("?q=a b")->to_string());
    println(u.with_query("")->to_string());

    net::query_params params;
    params.add("q", "c++ & go");
    params.add("page", "2");
    println(u.with_query(params)->to_string());
}
```

Output:

```text
https://x/search?q=a%20b#top
https://x/search#top
https://x/search?q=c%2B%2B+%26+go&page=2#top
```

## See also

- [query](query.md), [query_params](query_params.md): the query
- [sgcl::net::url](../url.md)
