[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::size

```cpp
size_t size() const noexcept;
```

Returns the number of keys in the record, each counted once.

## Parameters

None.

## Return value

The number of keys.

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
    net::dns_sd::txt_record txt = {{"a", "1"}, {"A", "2"}};
    txt.set("b");
    println("{}", txt.size());
}
```

Output:

```text
2
```

## See also

- [empty](empty.md): whether there is none
- [sgcl::net::dns_sd::txt_record](README.md)
