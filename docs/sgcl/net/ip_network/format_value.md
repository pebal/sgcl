[sgcl](../../README.md) › [net](../README.md) › [ip_network](README.md)

# sgcl::net::format_value (sgcl::net::ip_network)

```cpp
void format_value(txt::format_sink& out, const ip_network& network, const txt::format_spec& spec) noexcept;
```

Writes a network for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`), which
find it beside the class: `{}` writes [to_string](to_string.md), `"10.0.0.0/8"`, `"2001:db8::/32"`, in the width, the fill and the
alignment of the field as a [string](../../core/string/README.md) is written, to the left by default (`{:>20}`,
`{:*<20}`). Nothing is allocated for it.

Any type or precision, `{:x}` or `{:.3}`, is an error: of the compiler in a literal pattern, and `nullopt` from a
runtime one ([txt::runtime](../../txt/runtime.md)). Which specifications a network takes is said by the
specialization `txt::formatter<ip_network>`, for the pattern checked where it is compiled.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `network` | the network written |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

Linear in the length of the text, at most 59 bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    net::ip_network lan("192.168.0.0/16");
    println("{} holds 192.168.7.9: {}", lan, lan.contains(net::ip_address("192.168.7.9")));
    println("[{:^18}]", net::ip_network("10.0.0.0/8"));
    println("{}", txt::format(txt::runtime("{:.3}"), lan).has_value());
}
```

Output:

```text
192.168.0.0/16 holds 192.168.7.9: true
[    10.0.0.0/8    ]
false
```

## See also

- [to_string](to_string.md): the text written
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::net::ip_network](README.md)
