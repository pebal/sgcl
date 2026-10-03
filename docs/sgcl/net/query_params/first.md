[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::first

```cpp
static expected<string, io::error> first(const string& text, const string& name) noexcept;
```

What `parse(text).get(name)` gives, found in the text itself: the pairs are walked as [parse](parse.md) walks them, a
name is compared as it stands when it has nothing to decode, and only the value found is decoded. One value costs one
string, where `parse` makes two strings a pair; a server's request reads a query parameter by it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the query or the form body |
| `name` | the name, unescaped |

## Return value

The first value of `name`, unescaped, or the empty string when there is none; an [io::error](../../io/error.md) of the
code `net::errc::invalid_url` ([errc](../errc.md)), the operation `parse query` and the text, when the text is past
512 MiB ([the limit](../query_params.md#rules)). The pairs are not written, so their written length is not looked at.

## Complexity

Linear in the length of `text`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string query = "q=a+b&page=2&q=c";
    println("[{}] [{}] [{}]", *net::query_params::first(query, "q"), *net::query_params::first(query, "page"),
            *net::query_params::first(query, "lang"));
}
```

Output:

```text
[a b] [2] []
```

## See also

- [get](get.md): the same of parsed pairs
- [sgcl::net::query_params](../query_params.md)
