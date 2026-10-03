[sgcl](../../README.md) › [net](../README.md) › [url](../url.md)

# sgcl::net::format_value (sgcl::net::url)

```cpp
void format_value(txt::format_sink& out, const url& u, const txt::format_spec& spec) noexcept;
```

Writes a URL for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`), which
find it beside the class: `{}` writes [to_string](to_string.md), `"https://example.com/a%20b"`, in the width, the fill and the
alignment of the field as a [string](../../core/string.md) is written, to the left by default (`{:>20}`,
`{:*<20}`). Nothing is allocated for it.

Any type or precision, `{:x}` or `{:.3}`, is an error: of the compiler in a literal pattern, and `nullopt` from a
runtime one ([txt::runtime](../../txt/runtime.md)). Which specifications a URL takes is said by the
specialization `txt::formatter<url>`, for the pattern checked where it is compiled.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `u` | the URL written |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

Linear in the length of the URL's text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    net::url page("HTTPS://Example.com:443/a b?q=1");
    println("{}", page);
    println("[{:<20}]", net::url("http://a.test/"));
    println("{}", txt::format(txt::runtime("{:x}"), page).has_value());
}
```

Output:

```text
https://example.com/a%20b?q=1
[http://a.test/      ]
false
```

## See also

- [to_string](to_string.md): the text written
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::net::url](../url.md)
