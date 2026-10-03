[sgcl](../../README.md) › [net](../README.md) › [query_params](../query_params.md)

# sgcl::net::query_params::parse

```cpp
static expected<query_params, io::error> parse(const string& text) noexcept;
```

Reads the pairs of a query or of a form's body, `application/x-www-form-urlencoded`, as the standard reads them; Go's
`url.ParseQuery`, which keeps no order. A leading `?` is taken off first, as `URLSearchParams` does. The text is cut
at each `&`, an empty piece skipped, and each piece at its first `=` into a name and a value (a piece without one is a
name with the empty value). In both, a `+` is a space, an escape `%XX` is the byte it names and one that is not stays
as it is written, and bytes that are not UTF-8 after the unescaping become U+FFFD.

Every text is a list of pairs, as in the standard; the one refusal is [the limit](../query_params.md#rules) of 512 MiB,
of the text and of the pairs as [to_string](to_string.md) would write them.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the query or the form body |

## Return value

The pairs, in their order, or an [io::error](../../io/error.md) of the code `net::errc::invalid_url`
([errc](../errc.md)), the operation `parse query` and the text, when the text is past 512 MiB or its pairs would be
written past it (a byte the form escapes is three).

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
    auto params = net::query_params::parse("?a=1&b=x+y&&a=%zz&c&=d&e=%E2%82%AC");
    for (auto& [name, value] : *params) {
        println("[{}] = [{}]", name, value);
    }
}
```

Output:

```text
[a] = [1]
[b] = [x y]
[a] = [%zz]
[c] = []
[] = [d]
[e] = [€]
```

## See also

- [to_string](to_string.md): the pairs written back
- [first](first.md): one value, with nothing else parsed
- [url::query_params](../url/query_params.md): the pairs of a URL
- [sgcl::net::query_params](../query_params.md)
