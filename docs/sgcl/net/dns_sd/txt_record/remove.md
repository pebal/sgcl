[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::remove

```cpp
bool remove(const string& key) noexcept;
```

Takes a key out of the record, found without regard to case; the others keep their order.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the key was there.

## Complexity

Linear in the keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns_sd::txt_record txt = {{"a", "1"}, {"b", "2"}, {"c", "3"}};
    println("{} {}", txt.remove("B"), txt.remove("b"));
    for (auto& k : txt.keys()) {
        println("{}", k);
    }
}
```

Output:

```text
true false
a
c
```

## See also

- [set](set.md): the other way
- [sgcl::net::dns_sd::txt_record](README.md)
