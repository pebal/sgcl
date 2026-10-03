[sgcl](../../README.md) › [net](../README.md) › [endpoint](../endpoint.md)

# sgcl::net::format_value (sgcl::net::endpoint)

```cpp
void format_value(txt::format_sink& out, const endpoint& e, const txt::format_spec& spec) noexcept;
```

Writes an endpoint for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`), which
find it beside the class: `{}` writes [to_string](to_string.md), `"10.0.0.1:80"`, `"[::1]:443"`, in the width, the fill and the
alignment of the field as a [string](../../core/string.md) is written, to the left by default (`{:>20}`,
`{:*<20}`). Nothing is allocated for it.

Any type or precision, `{:x}` or `{:.3}`, is an error: of the compiler in a literal pattern, and `nullopt` from a
runtime one ([txt::runtime](../../txt/runtime.md)). Which specifications an endpoint takes is said by the
specialization `txt::formatter<endpoint>`, for the pattern checked where it is compiled.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `e` | the endpoint written |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

Linear in the length of the text, at most 63 bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    net::endpoint web("[::1]:443");
    println("dial {}", web);
    println("[{:>14}]", net::endpoint("10.0.0.1:80"));
    println("{}", txt::format(txt::runtime("{:d}"), web).has_value());
}
```

Output:

```text
dial [::1]:443
[   10.0.0.1:80]
false
```

## See also

- [to_string](to_string.md): the text written
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::net::endpoint](../endpoint.md)
