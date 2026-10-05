[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::operator==, operator!= (sgcl::net::dns_sd::txt_record)

```cpp
friend bool operator==(const txt_record& a, const txt_record& b) noexcept;
```

Checks whether `a` and `b` hold the same entries in the same order, byte for byte: the keys' case counts here,
as the order does. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the records to compare |

## Return value

`true` when the entries are the same.

## Complexity

Linear in the entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns_sd::txt_record a = {{"x", "1"}, {"y", "2"}};
    net::dns_sd::txt_record b = {{"x", "1"}, {"y", "2"}};
    net::dns_sd::txt_record c = {{"y", "2"}, {"x", "1"}};
    println("{} {}", a == b, a != c);
}
```

Output:

```text
true true
```

## See also

- [entries](entries.md): what is compared
- [sgcl::net::dns_sd::txt_record](README.md)
