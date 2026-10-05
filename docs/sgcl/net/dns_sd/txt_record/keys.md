[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::keys

```cpp
vector<string> keys() const noexcept;
```

Returns the keys of the record in their order, each as it was last set.

## Parameters

None.

## Return value

The keys, empty for an empty record.

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
    net::dns_sd::txt_record txt = {{"path", "/"}, {"v", "1"}};
    txt.set("secure");
    txt.set("V", "2");
    for (auto& k : txt.keys()) {
        println("{}", k);
    }
}
```

Output:

```text
path
V
secure
```

## See also

- [entries](entries.md): the keys with their values
- [sgcl::net::dns_sd::txt_record](README.md)
