[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::set

```cpp
void set(const string& key, const string& value);    // (1)
void set(const string& key);                         // (2)
```

Sets a key (RFC 6763 §6.4): a key already there, without regard to case, keeps its place and takes the new entry,
written with the key as given now; a new one goes at the end.

1. The key with its value, the entry `"key=value"`: an empty value is a value (`"key="`).
2. The key alone, the entry `"key"`: an attribute present with no value, [get](get.md) of which is `nullopt`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | printable ASCII (`0x20`–`0x7E`) without `=`, at least one character |
| `value` | any bytes |

## Return value

None.

## Complexity

Linear in the keys.

## Exceptions

`invalid_argument` for a key that is empty, holds `=` or a byte outside `0x20`–`0x7E`, and for an entry of more
than 255 bytes. The record is left as it was.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

#include <stdexcept>

using namespace sgcl;

int main() {
    net::dns_sd::txt_record txt;
    txt.set("path", "/");
    txt.set("empty", "");
    txt.set("secure");
    txt.set("PATH", "/v2");
    for (auto& e : txt.entries()) {
        println("{}", e);
    }
    try {
        txt.set("k", string(255, 'x'));  // "k=" and 255 bytes: 257
    } catch (const std::invalid_argument& e) {
        println("{}", txt.size());
    }
}
```

Output:

```text
PATH=/v2
empty=
secure
3
```

## See also

- [get](get.md): a value back
- [remove](remove.md): a key taken out
- [sgcl::net::dns_sd::txt_record](README.md)
