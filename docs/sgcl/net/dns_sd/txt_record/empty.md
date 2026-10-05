[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::empty

```cpp
bool empty() const noexcept;
```

Checks whether the record holds no key. An empty record is published as one empty string (RFC 6763 §6.1).

## Parameters

None.

## Return value

`true` when there is no key.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns_sd::txt_record txt;
    println("{}", txt.empty());
    txt.set("a");
    println("{}", txt.empty());
}
```

Output:

```text
true
false
```

## See also

- [size](size.md): how many
- [sgcl::net::dns_sd::txt_record](README.md)
