[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::contains

```cpp
bool contains(const string& key) const noexcept;
```

Checks whether a key is in the record, with a value or alone, compared without regard to case.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the key is there.

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
    net::dns_sd::txt_record txt;
    txt.set("Secure");
    println("{} {}", txt.contains("secure"), txt.contains("insecure"));
}
```

Output:

```text
true false
```

## See also

- [get](get.md): its value
- [sgcl::net::dns_sd::txt_record](README.md)
