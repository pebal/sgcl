[sgcl](../../README.md) › [net](../README.md) › [ip_address](../ip_address.md)

# sgcl::net::format_value (sgcl::net::ip_address)

```cpp
void format_value(txt::format_sink& out, const ip_address& address, const txt::format_spec& spec) noexcept;
```

Writes an address for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`), which
find it beside the class: `{}` writes [to_string](to_string.md), `"10.0.0.1"`, `"fe80::1%en0"`, in the width, the fill and the
alignment of the field as a [string](../../core/string.md) is written, to the left by default (`{:>20}`,
`{:*<20}`). Nothing is allocated for it.

Any type or precision, `{:x}` or `{:.3}`, is an error: of the compiler in a literal pattern, and `nullopt` from a
runtime one ([txt::runtime](../../txt/runtime.md)). Which specifications an address takes is said by the
specialization `txt::formatter<ip_address>`, for the pattern checked where it is compiled.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `address` | the address written |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

Linear in the length of the text, at most 55 bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    net::ip_address scoped("fe80::1%en0");
    println("{} and {}", scoped, net::ip_address::loopback_v6());
    println("[{:>12}] [{:*<12}]", net::ip_address("10.0.0.1"), net::ip_address("::1"));
    println("{}", txt::format(txt::runtime("{:x}"), scoped).has_value());
}
```

Output:

```text
fe80::1%en0 and ::1
[    10.0.0.1] [::1*********]
false
```

## See also

- [to_string](to_string.md): the text written
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::net::ip_address](../ip_address.md)
