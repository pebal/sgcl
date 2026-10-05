[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::entries

```cpp
const vector<string>& entries() const noexcept;
```

Returns the entries of the record as it carries them on the wire, each a string of the TXT: `"key=value"`, or
`"key"` for a key alone, in their order.

## Parameters

None.

## Return value

The entries, a reference into the record, valid until it changes.

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
    net::dns_sd::txt_record txt = {{"path", "/api"}};
    txt.set("secure");
    for (auto& e : txt.entries()) {
        println("{} ({} bytes)", e, e.size());
    }
}
```

Output:

```text
path=/api (9 bytes)
secure (6 bytes)
```

## See also

- [keys](keys.md): the keys alone
- [sgcl::net::dns_sd::txt_record](README.md)
